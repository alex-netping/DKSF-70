/* v2 by LBS 05.2009
*  ѕробелы вокруг провер€емых значений считаютс€ ошибкой!
*  Multicast ip и mac не провер€етс€
*/   

function IsIP(ip,msg)
{ try 
 {
 if(!(/^((\d{1,3})\.){3}(\d{1,3})$/).test(ip)) throw(3);
 a=ip.split('.'); for(n in a) if(!a[n] || a[n]>255) throw(5);
 if(ip=="255.255.255.255") throw(255);
 if(a[0]==255) throw(0);
 return true;
 } 
 catch(e)
 { 
 if(!msg) msg="'"+ip+"' неправильный IP адрес";  
 if(e==0 || e==255) msg+=". Ўироковещательные и специальные IP адреса недопустимы.";
 alert(msg);
 return false;
 }
}

function IsLikeOnIP(ip,msg)
{ try 
 {
 if(!(/^((\d{1,3})\.){3}(\d{1,3})$/).test(ip)) throw(3);
 a=ip.split('.'); for(n in a) if(!a[n] || a[n]>255) throw(5);
 return true;
 } 
 catch(e)
 {
 if(!msg) msg="'"+ip+"' неправильный IP адрес";  
 alert(msg);
 return false;
 }
}

function macsCheck(mac,msg) {
var r=(/^([0-9a-f]{2}:){5}[0-9a-f]{2}$/i).test(mac);
var m2; if((/^ff:ff:ff:ff:ff:ff$/i).test(mac)) { r=false; m2=" Ўироковещательный адрес недопустим!"; }
if(!r) {  
 if(!msg) msg="'"+num+"' неправильный MAC адрес" + ( m2 ? "." : " (используйте форму xx:xx:xx:xx:xx:xx)" );
 if(m2) msg+=m2;
 alert(msg);
}
return r;
}

function validNumCheck(num,msg) {  
var r = (/^[0-9]+$/).test(num);
if(!r) {  
 if(!msg) msg="'"+num+"' неправильное число"; 
 alert(msg);
}
return r;
}

function validLatCheck(str,msg) {
var r = (/^[a-z_]+$/i).test(str);
if(!r) {  
 if(!msg) msg="'"+str+"' допустимы только латинские буквы"; 
 alert(msg);
}
return r;
}

function validLatNumCheck(str,msg) {
var r = (/^[a-z][a-z0-9_]*$/i).test(str);
if(!r) {  
 if(!msg) msg="'"+str+"' неправильный идентификатор (лат.буквы и цифры)"; 
 alert(msg);
}
return r;
}