<!--
//======================================================================
// November 2005 changes, ver 4.01
// added numonly and numlet functions to stop entry of invalid characters
//
// ======================================================================
var deg2rad = Math.PI / 180;
var rad2deg = 180.0 / Math.PI;
var pi = Math.PI;
var locres = 0; //6 locator characters
domain = 'cix.co.uk';
user = 'hzk';

onload = function()
{
document.myform.reset();
//document.myform.loc1.focus();
}

function onTypeLoc()
{
   console.log(document.myform.loc2.value.length);
  if (document.myform.loc2.value.length >= 6)
    {
	  find_dist();
	}  
}

//-------------------------------------------------------------------
// this function allows only numbers in fields
function numonly(ev)
{
  var key;
  var keychar;
  if (window.event)
	key = window.event.keycode;
  else if (ev)
	key = ev.which;
  else return true;
  keychar = String.fromCharCode(key);
  if ((key==null) || (key==0) || (key==8) || (key==9) || (key==13) || (key==27))
	return true;
  else if (((".0123456789").indexOf(keychar) > -1))
	return true;
  else
	return false;
}
//-------------------------------------------------------------------
// this function allows numbers, letters, spaces in fields
function numlet(ev)
{
  var key;
  var keychar;
  if (window.event)
	key = window.event.keycode;
  else if (ev)
	key = ev.which;
  else return true;
  keychar = String.fromCharCode(key);
  keychar = keychar.toLowerCase();
  if ((key==null) || (key==0) || (key==8) || (key==9) || (key==13) || (key==27))
	return true;
  else if ((("abcdefghijklmnopqrstuvwxyz .0123456789").indexOf(keychar) > -1))
	return true;
  else
	return false;
}
//-------------------------------------------------------------------
function find_dist()
{
var loc1 = document.myform.loc1.value.toUpperCase();
var loc2 = document.myform.loc2.value.toUpperCase();
var loc1x = loc1;
var loc2x = loc2;
var valid = 1;  //1=valid 0=invalid

loc1 = expand_loc(loc1);
loc2 = expand_loc(loc2);
if (validate2(loc1, loc2) == 0)
  {
  alert("Неверный формат локатора");
  return;
  }

document.myform.reset();
document.myform.loc1.value = loc1x;
document.myform.loc2.value = loc2x;

geo1 = conv_loc_to_deg(loc1);
geo2 = conv_loc_to_deg(loc2);

lon1 = geo1.longitude * deg2rad;
lat1 = geo1.latitude * deg2rad;

lon2 = geo2.longitude * deg2rad;
lat2 = geo2.latitude * deg2rad;

calc_rl(lat1, -lon1, lat2, -lon2);
}

//-------------------------------------------------------------------
function calc_gc(lat1, lon1, lat2, lon2)
{
var d = acos(sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(lon1 - lon2));
var gc_d = round((rad2deg * d) * 60 * 10) / 10;
var gc_dm = round(1.852 * gc_d * 10) / 10;
var gc_ds = round(1.150779 * gc_d * 10) / 10;

if (sin(lon2 - lon1) < 0)
  tc = acos((sin(lat2) - sin(lat1) * cos(d)) / (sin(d) * cos(lat1)));
else if (lon2 - lon1 == 0)
  if (lat2 < lat1)
	tc = deg2rad * 180;
  else
	tc = 0;
else
  tc = 2 * pi - acos((sin(lat2) - sin(lat1) * cos(d)) / (sin(d) * cos(lat1)));

gc_tc = round(tc * rad2deg * 10) / 10;

document.myform.gc_km.value = gc_dm;
}
//-------------------------------------------------------------------
function calc_rl(lat1, lon1, lat2, lon2)
{
var dlon_w = mod(lon2 - lon1, pi * 2);
var dlon_e = mod(lon1 - lon2, pi * 2);

var dphi = ln(tan(lat2 / 2 + pi / 4) / tan(lat1 / 2 + pi / 4));

if (abs(lat2 - lat1) < sqrt(1e-15))
  var q = cos(lat1);
else
  var q = (lat2 - lat1) / dphi;

if (dlon_w < dlon_e)
  {
  var tc = mod(atan2(-dlon_w, dphi), 2.0 * pi);
  var d = sqrt(q * q * dlon_w * dlon_w + (lat2 - lat1) * (lat2 - lat1));
  }
else
  {
  var tc = mod(atan2(dlon_e, dphi), 2.0 * pi);
  var d = sqrt(q * q * dlon_e * dlon_e + (lat2 - lat1) * (lat2 - lat1));
  }

var rl_d = round(rad2deg * d * 60 * 10) / 10;
var rl_dm = round(1.852 * rl_d * 10) / 10;
var rl_ds = round(1.150779 * rl_d * 10) / 10;
var rl_tc = round(rad2deg * tc * 10) / 10;

document.myform.rl_deg.value = rl_tc;
document.myform.rl_km.value = rl_dm;
document.getElementById("neededazimuth").value = Math.trunc(rl_tc);
}
//-------------------------------------------------------------------
function expand_loc(original)
{
 var expanded = original;
 if (original.length == 6)
	 expanded = original + "55AA";
 if (original.length == 8)
	expanded = original + "LL";
 return(expanded);
}
//-------------------------------------------------------------------
function validate2(first, second)
{
 if ((first.length != 10) || (second.length != 10))
  return(0);

 // check locator format
 var myRegExp = /[A-R]{2}[0-9]{2}[A-X]{2}[0-9]{2}[A-X]{2}/;
 if ((! myRegExp.test(first)) || (! myRegExp.test(second)))
 return(0);
}
//-------------------------------------------------------------------
function conv_loc_to_deg(locator)
{
 var i = 0;
 var loca = new Array();

 while (i < 10)
  {
  loca[i] = locator.charCodeAt(i) - 65;
  i++;
  }
 loca[2] += 17;
 loca[3] += 17;
 loca[6] += 17;
 loca[7] += 17;
 var lon = (loca[0] * 20 + loca[2] * 2 + loca[4] / 12 + loca[6] / 120 + loca[8] / 2880 - 180);
 var lat = (loca[1] * 10 + loca[3] + loca[5] / 24 + loca[7] / 240 + loca[9] /5760 - 90);
 var geo = { latitude: lat, longitude: lon };
 return(geo);
}

// Maths functions

function mod(y, x)
{
if (y >= 0)
  return y - x * floor(y / x);
else
  return y + x * (floor(-y / x) + 1.0);
}

function atan2(y, x)
{
	return Math.atan2(y, x);
}

function sqrt(x)
{
	return Math.sqrt(x);
}

function tan(x)
{
	return Math.tan(x);
}

function sin(x)
{
	return Math.sin(x);
}

function cos(x)
{
	return Math.cos(x);
}

function acos(x)
{
	return Math.acos(x);
}

function floor(x)
{
	return Math.floor(x);
}

function round(x)
{
	return Math.round(x);
}

function ceil(x)
{
	return Math.ceil(x)
}

function ln(x)
{
	return Math.log(x);
}

function abs(x)
{
	return Math.abs(x);
}

function pow(x, y)
{
	return Math.pow(x, y);
}

function atan(x)
{
	return Math.atan(x);
}

function chr(x)
{
return String.fromCharCode(x);
}

function round(x)
{
	return Math.round(x);
}

//-->
// End of myform
//===================================================================