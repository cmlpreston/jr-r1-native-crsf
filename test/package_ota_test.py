import importlib.util
from pathlib import Path
import hashlib
import struct
import unittest
spec=importlib.util.spec_from_file_location('package_ota',Path(__file__).resolve().parents[1]/'tools/package_ota.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
class PackageTest(unittest.TestCase):
    def test_round_trip(self):
        image=bytearray(4096);image[0]=0xe9;struct.pack_into('<H',image,12,9)
        package=module.package(image)
        magic,target,size,md5=struct.unpack('<8s32sI32s',package[:76])
        self.assertEqual(magic,b'R1OTA001')
        self.assertEqual(target.rstrip(b'\0'),module.TARGET)
        self.assertEqual(size,len(image))
        self.assertEqual(md5.decode(),hashlib.md5(image).hexdigest())
        self.assertEqual(package[76:],image)
    def test_reject_other_images(self):
        for image in (b'',bytes(100),bytes(4096),b'\xe9'+bytes(4095)):
            with self.assertRaises(ValueError):module.package(image)
if __name__=='__main__':unittest.main()
