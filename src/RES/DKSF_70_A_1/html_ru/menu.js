function menu(page_name)
{
/*
if(typeof(window.sys_name)=='undefined') window.sys_name='';
if(typeof(window.sys_location)=='undefined') window.sys_location='';
*/
l ='<div id="logo" style="padding-right:30px">';
l+='<h1>'+devname+'</h1>';
l+='<table style="width:100%"><tr>';
l+='<td style="vertical-align:bottom;text-align:left;border:0px">'+page_name+'</td>';
l+='<td style="vertical-align:bottom;text-align:right;border:0px;font-size:80%">'+sys_name;
if(sys_location) l+='<br/>'+sys_location;
l+='</td></tr></table></div>';
l+='<div id="menu">';
if(hwmodel==70)
{
l+=[
'<a href="index.html">�������</a>',
'<a href="settings.html">���������</a>',
'<a href="sendmail.html">e-mail</a>',
'<a href="sms.html">SMS</a>',
'<a href="com.html">COM ����</a>',
'<a href="ow.html">1-Wire</a>',
'<a href="termo.html">������������</a>',
'<a href="rh.html">������ ���������</a>',
'<a href="update.html">��������</a>',
'<a href="log.html">������</a>'
].join(' | ')+'<br/>';
}
else // dksf 71
{
l+=[
'<a href="index.html">�������</a>',
'<a href="settings.html">���������</a>',
'<a href="sendmail.html">e-mail</a>',
'<a href="ow.html">1-Wire</a>',
'<a href="termo.html">������������</a>',
'<a href="rh.html">������ ���������</a>',
'<a href="update.html">��������</a>',
'<a href="log.html">������</a>'
].join(' | ')+'<br/>';
}
l+=[
'<a href="io.html">����-�����</a>',
'<a href="relay.html">���������� ����</a>',
'<a href="wdog.html">������</a>',
'<a href="wtimer.html">����������</a>',
'<a href="curdet.html">������ ����</a>',
'<a href="ir.html">�� �������</a>',
'<a href="logic.html">������</a>'
].join(' | ');
if(typeof nf_disable != 'undefined' && nf_disable)
{
 l+='<div id="nonf" style="margin-top:8px">';
 l+='<span class="alert" style="text-transform:none;font-family:serif">����������� ���������! <a href="settings.html#frm_nf_disable_id">������������</a></span>';
 l+='</div>';
}
l+='</div>';
document.write(l);
}

