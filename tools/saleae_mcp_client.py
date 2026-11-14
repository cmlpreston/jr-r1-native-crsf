import urllib.request,json,pathlib
STATE=pathlib.Path('/tmp/r1-saleae-mcp-state.json')
def request(method,params=None,ident=1,state=None):
 payload={'jsonrpc':'2.0','method':method}
 if ident is not None:payload['id']=ident
 if params is not None:payload['params']=params
 headers={'Content-Type':'application/json','Accept':'application/json, text/event-stream'}
 if state:
  if state.get('session'):headers['Mcp-Session-Id']=state['session']
  headers['MCP-Protocol-Version']=state.get('protocol','2024-11-05')
 req=urllib.request.Request('http://127.0.0.1:10530',data=json.dumps(payload).encode(),headers=headers)
 with urllib.request.urlopen(req,timeout=15) as r:
  session=r.headers.get('Mcp-Session-Id')
  if r.status==202 or ident is None:return None,session
  if 'text/event-stream' in r.headers.get('Content-Type',''):
   for line in r:
    if line.startswith(b'data: '):return json.loads(line[6:]),session
  else:return json.load(r),session
 return None,session
def initialize():
 result,session=request('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'r1-esc-bench','version':'1'}})
 if 'error' in result:raise RuntimeError(result['error'])
 state={'session':session,'protocol':result['result']['protocolVersion']};STATE.write_text(json.dumps(state));STATE.chmod(0o600)
 request('notifications/initialized',ident=None,state=state)
 return result['result']
def rpc(method,params=None):
 state=json.loads(STATE.read_text());result,_=request(method,params,state=state)
 if result and 'error' in result:raise RuntimeError(result['error'])
 return result.get('result') if result else None
if __name__=='__main__':
 print('Server:',initialize())
 data=rpc('tools/list');pathlib.Path('/tmp/r1-saleae-mcp-tools.json').write_text(json.dumps(data,indent=2))
 for t in data['tools']:print(t['name'],t.get('description','')[:230])
