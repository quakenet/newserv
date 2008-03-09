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
  "sendto": {
    5: "prevemail",
  },
  "languages": {
    "en": {
      1: {
        "subject": "%(config.bot)s account registration",
        "body": """
Thank you for registering.
To get your password please visit:
%(url)s

In case you forget your login/password use:
/msg %(config.bot)s REQUESTPASSWORD %(user.username)s %(user.email)s

Make sure you've read the %(config.bot)s FAQ at %(config.siteurl)s for a complete
reference on Q's commands and usage.

 ** PLEASE READ %(config.securityurl)s --
    it contains important information about keeping your account secure.
    Note that QuakeNet Operators will not intervene if you fail to read
    the above URL and your account is compromised as a result.

PLEASE REMEMBER THAT UNUSED ACCOUNTS ARE AUTOMATICALLY REMOVED
AFTER %(config.cleanup)d DAYS, AND ALL CHANLEVS ARE LOST!

NB: Save this email for future reference.
""",
      },
      2: { "subject": "%(config.bot)s password request", "body": """
Your username/password is:

Username: %(user.username)s
Password: %(user.password)s

To auth yourself to %(config.bot)s, type the following command

   /MSG %(config.bot)s@%(config.server)s AUTH %(user.username)s %(user.password)s
""", },
      3: { "subject": "%(config.bot)s password change", "body": "Your new password: %(user.password)s", },
      5: { "subject": "%(config.bot)s email change", "body": """
Your email address has been changed on %(config.bot)s.

ADD RESET STUFF,

blah %(user.email)s
""", },
    },
  },
}
