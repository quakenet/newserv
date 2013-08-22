class RC4:
  def __init__(self, key, burn=4096):
    s = range(256)
    for i in xrange(256):
      s[i] = i
    j = 0
    for i in xrange(256):
      j = (j + s[i] + ord(key[i % len(key)])) % 256
      s[j], s[i] = s[i], s[j]
    self.__s = s
    self.crypt("\x00" * burn)

  def crypt(self, data):
    ret = []
    i = 0
    j = 0
    for r in xrange(len(data)):
      i = (i + 1) % 256
      j = (j + self.__s[i]) % 256
      self.__s[i], self.__s[j] = self.__s[j], self.__s[i]
      ret.append(chr(ord(data[r]) ^ self.__s[(self.__s[i] + self.__s[j]) % 256]))
    return "".join(ret)


