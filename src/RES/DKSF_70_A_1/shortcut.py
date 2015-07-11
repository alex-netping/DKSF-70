from win32com.shell import shell
import pythoncom
shortcut = pythoncom.CoCreateInstance(\
	shell.CLSID_ShellLink, None,\
	pythoncom.CLSCTX_INPROC_SERVER, shell.IID_IShellLink)

def resolve(lnk_file):
  shortcut.QueryInterface( pythoncom.IID_IPersistFile ).Load( lnk_file )
  return shortcut.GetPath(0,256)[0]


