"""PlatformIO custom upload action; upload_preflight.py runs before this."""
Import('env')
import json, os, pathlib, runpy, urllib.request
root=pathlib.Path(env.subst('$PROJECT_DIR')).resolve().parents[2]
checks=runpy.run_path(str(root/'tools/er4_upload_preflight.py'))
def upload(source,target,env):
    artifacts=pathlib.Path(os.environ['JR_R1_ER4_ARTIFACT_DIR'])
    checks['validate'](root,env.subst('$PROJECT_DIR'),env.subst('$BOARD'),artifacts,
        direct_read=env.subst('$PIOENV')=='er4-esc-direct-read-wifi')
    data=(artifacts/'er4-esc-rf-only-bench.bin.gz').read_bytes()
    boundary='R1Er4RfOnlyBenchUpload'
    body=('--'+boundary+'\r\nContent-Disposition: form-data; name="data"; filename="firmware.bin.gz"\r\nContent-Type: application/octet-stream\r\n\r\n').encode()+data+('\r\n--'+boundary+'--\r\n').encode()
    request=urllib.request.Request('http://'+checks['HOST']+'/update',data=body,headers={'Content-Type':'multipart/form-data; boundary='+boundary,'X-FileSize':str(len(data))},method='POST')
    # Standard ESP8266 compressed OTA. No force flag, no automatic write retry.
    with urllib.request.urlopen(request,timeout=55) as response:result=json.load(response)
    (artifacts/'upload-response.json').write_text(json.dumps(result,indent=2)+'\n')
    print('ER4 OTA result:',result.get('status'));print(result.get('msg',''))
    return 0 if result.get('status')=='ok' else 1
env.Replace(UPLOAD_PROTOCOL='custom',UPLOADCMD=upload)

# Stage only the already verified image after PlatformIO prepares its build dir.
# No compilation, serial-port detection or reset is performed by this project.
artifact_dir=pathlib.Path(os.environ['JR_R1_ER4_ARTIFACT_DIR'])
staged=pathlib.Path(env.subst('$BUILD_DIR/${PROGNAME}.bin'))
staged.parent.mkdir(parents=True,exist_ok=True)
staged.write_bytes((artifact_dir/'er4-esc-rf-only-bench.bin').read_bytes())
staged.chmod(0o600)
