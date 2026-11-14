"""Read-only ESP32 application digest check. Never flashes or changes headers."""
import hashlib,struct
def app_digest(image):
 if len(image)<24 or image[0]!=0xe9 or image[23]!=1 or not 1<=image[1]<=16:
  raise ValueError('Require ESP32 application image with appended SHA256')
 end=24
 for _ in range(image[1]):
  if end+8>len(image):raise ValueError('Truncated ESP segment header')
  length=struct.unpack_from('<I',image,end+4)[0];end+=8+length
  if end>len(image):raise ValueError('Truncated ESP segment')
 end=(end//16+1)*16
 digest=hashlib.sha256(image[:end]).digest()
 if image[end:end+32]!=digest:raise ValueError('ESP application hash mismatch')
 return digest.hex()
