#!/usr/bin/env python3
"""PlatformIO OTA upload transport; invoke through the upload target/preflight."""
import base64
import getpass
import json
import os
from pathlib import Path
import sys
import urllib.request
from package_ota import package

def main():
    base=os.environ['JR_R1_OTA_URL'].rstrip('/')
    if not base.startswith('http://'):
        raise ValueError('Use the handset HTTP address on a trusted LAN or SSH tunnel')
    password=os.environ.get('R1_WEB_PASSWORD') or getpass.getpass('Handset web password: ')
    auth='Basic '+base64.b64encode(('r1:'+password).encode()).decode()
    def request(path,data=None,headers=None):
        req=urllib.request.Request(base+path,data=data,headers={'Authorization':auth,**(headers or {})})
        with urllib.request.urlopen(req,timeout=180) as r:return r.status,r.read()
    _,body=request('/api/state')
    state=json.loads(body)
    if state.get('controlAllowed') is not True:
        raise ValueError('Handset must be CUT before firmware upload')
    image=package(Path(sys.argv[1]).read_bytes())
    boundary='r1firmware'+os.urandom(12).hex()
    body=(f'--{boundary}\r\nContent-Disposition: form-data; name="firmware"; filename="firmware.r1ota"\r\nContent-Type: application/octet-stream\r\n\r\n'.encode()+image+f'\r\n--{boundary}--\r\n'.encode())
    status,result=request('/api/firmware',body,{'X-R1-Token':state['token'],'Content-Type':'multipart/form-data; boundary='+boundary})
    if status!=202:raise ValueError('No firmware acceptance confirmation')
    print(result.decode())

if __name__=='__main__':main()
