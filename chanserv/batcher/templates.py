import md5, os
from rc4 import RC4

def generate_url(config, obj):
  s = os.urandom(4)
  r = RC4(md5.md5("%s %s" % (s, config["urlkey"])).hexdigest())
  a = r.crypt(obj["user.password"])
  b = md5.md5(md5.md5("%s %s %s %s" % (config["urlsecret"], obj["user.username"], a, s)).hexdigest()).hexdigest()
  obj["url"] = "%s?m=%s&h=%s&u=%s&r=%s" % (config["url"], a.encode("hex"), b, obj["user.username"].encode("hex"), s.encode("hex"))

MAILTEMPLATES = {
  "mutators": {
    1: generate_url,
  },

  "languages": {
    "en": {
      1: {
        "subject": "New account registration",
        "body": """
Thank you for registering.

Username: %(user.username)s
Please visit %(url)s to get your password.
""",
      },
      2: { "subject": "Password request", "body": "Your password: %(user.password)s", },
      3: { "subject": "New password", "body": "Your new password: %(user.password)s", },
      5: { "subject": "Email change", "body": "Your old email address: %(prevemail)s", },
    },
  },
}
