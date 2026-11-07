import hashlib, sys
p = sys.argv[1]
d = open(p, "rb").read()
print("tamanho:", len(d))
print("sha256:", hashlib.sha256(d).hexdigest())
print("primeiros 16 bytes:", d[:16].hex(" "))
print("texto:", d[:16])