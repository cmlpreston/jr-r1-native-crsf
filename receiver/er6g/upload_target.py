"""PlatformIO custom upload action; upload_preflight.py runs before this."""
Import('env')
import json, os, pathlib, runpy, urllib.request
root=pathlib.Path(env.subst('$PROJECT_DIR')).resolve().parents[2]
is_write=env.subst('$PIOENV') in ('er6g-esc-write-wifi','er6g-esc-write-startup-wifi')
is_single=env.subst('$PIOENV')=='er6g-esc-single-wifi'
is_restore=env.subst('$PIOENV') in ('er6g-esc-restore-wifi','er6g-esc-restore-startup-wifi')
is_mode=is_write or is_single or is_restore or env.subst('$PIOENV') in ('er6g-esc-mode-wifi','er6g-esc-mode-startup-wifi')
mode_startup=env.subst('$PIOENV') in ('er6g-esc-mode-startup-wifi','er6g-esc-restore-startup-wifi','er6g-esc-write-startup-wifi')
is_startup=env.subst('$PIOENV')=='er6g-esc-startup-wifi'
checks=runpy.run_path(str(root/('tools/er6g_write_upload_preflight.py' if is_write else 'tools/er6g_restore_upload_preflight.py' if is_restore else 'tools/er6g_single_upload_preflight.py' if is_single else 'tools/er6g_mode_upload_preflight.py' if is_mode else 'tools/er6g_startup_upload_preflight.py' if is_startup else 'tools/er6g_upload_preflight.py')))
def validate_image(artifacts):
    if is_mode:return checks['validate'](root,env.subst('$PROJECT_DIR'),env.subst('$BOARD'),artifacts,startup=mode_startup)
    if is_startup:return checks['validate'](root,env.subst('$PROJECT_DIR'),env.subst('$BOARD'),artifacts)
    return checks['validate'](root,env.subst('$PROJECT_DIR'),env.subst('$BOARD'),artifacts,listen_only=env.subst('$PIOENV')=='er6g-esc-listen-wifi',read_only=env.subst('$PIOENV')=='er6g-esc-read-wifi')
def selected_image_name():
    if is_mode:return checks['image_name'](startup=mode_startup)
    return checks['image_name'](env.subst('$PIOENV')=='er6g-esc-listen-wifi',env.subst('$PIOENV')=='er6g-esc-read-wifi')
def upload(source,target,env):
    artifacts=pathlib.Path(os.environ['JR_R1_ER6G_ARTIFACT_DIR'])
    validate_image(artifacts)
    data=(artifacts/selected_image_name()).read_bytes()
    boundary='R1Er6gSlotFixUpload'
    body=('--'+boundary+'\r\nContent-Disposition: form-data; name="data"; filename="firmware.bin"\r\nContent-Type: application/octet-stream\r\n\r\n').encode()+data+('\r\n--'+boundary+'--\r\n').encode()
    request=urllib.request.Request('http://'+checks['HOST']+'/update',data=body,headers={'Content-Type':'multipart/form-data; boundary='+boundary,'X-FileSize':str(len(data))},method='POST')
    # Standard ESP32 raw-image OTA. No force flag, no automatic write retry.
    with urllib.request.urlopen(request,timeout=55) as response:result=json.load(response)
    (artifacts/'upload-response.json').write_text(json.dumps(result,indent=2)+'\n')
    print('ER6G OTA result:',result.get('status'));print(result.get('msg',''))
    return 0 if result.get('status')=='ok' else 1
env.Replace(UPLOAD_PROTOCOL='custom',UPLOADCMD=upload)

# Stage only the already verified image after PlatformIO prepares its build dir.
# No compilation, serial-port detection or reset is performed by this project.
artifact_dir=pathlib.Path(os.environ['JR_R1_ER6G_ARTIFACT_DIR'])
staged=pathlib.Path(env.subst('$BUILD_DIR/${PROGNAME}.bin'))
staged.parent.mkdir(parents=True,exist_ok=True)
staged.write_bytes((artifact_dir/selected_image_name()).read_bytes())
staged.chmod(0o600)
