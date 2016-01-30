import md5, os, hmac, time
from rc4 import RC4

try:
  import hashlib
  sha256 = hashlib.sha256
  sha256m = hashlib.sha256
except ImportError:
  import sha256 as __sha256
  sha256 = __sha256.sha256
  sha256m = __sha256

def generate_url(config, obj):
  s = os.urandom(4)
  r = RC4(md5.md5("%s %s" % (s, config["urlkey"])).hexdigest(), burn=0)
  a = r.crypt(obj["user.password"])
  b = md5.md5(md5.md5("%s %s %s %s" % (config["urlsecret"], obj["user.username"], a, s)).hexdigest()).hexdigest()
  obj["url"] = "%s?m=%s&h=%s&u=%s&r=%s" % (config["url"], a.encode("hex"), b, obj["user.username"].encode("hex"), s.encode("hex"))

def generate_activation_url(config, obj):
  r = os.urandom(16).encode("hex")
  uid = int(obj["user.id"])

  r2 = RC4(sha256("rc4 %s %s" % (r, config["activationkey"])).hexdigest())
  a = r2.crypt(obj["user.password"]).encode("hex")

  h = hmac.HMAC(sha256("hmac %s %s" % (r, config["activationkey"])).hexdigest())
  h.update("%d %s %s" % (uid, obj["user.username"], a))
  hd = h.hexdigest()

  obj["url"] = "%s?id=%d&h=%s&r=%s&u=%s&p=%s" % (config["activationurl"], uid, hd, r, obj["user.username"].encode("hex"), a)

def generate_resetcode(config, obj):
  if obj["user.lockuntil"] == 0:
    obj["resetline"] = "LOCK UNTIL NOT SET. STAFF ACCOUNT'S CAN'T USE RESET"
    obj["lockuntil"] = "never"
    return

  if not config.has_key("__codegensecret"):
    config["__codegenhmac"] = hmac.HMAC(key=sha256(sha256("%s:codegenerator" % config["q9secret"]).digest()).hexdigest(), digestmod=sha256m)

  h = config["__codegenhmac"].copy()
  h.update(sha256("%s:%d" % (obj["user.username"], obj["user.lockuntil"])).hexdigest())

  obj["resetcode"] = h.hexdigest()
  obj["lockuntil"] = time.ctime(obj["user.lockuntil"])
  obj["resetline"] = "/MSG %(config.bot)s RESET #%(user.username)s %(resetcode)s" % obj

def generate_resetpassword(config, obj):
  generate_resetcode(config, obj)
  obj["resetline"] = "/MSG %(config.botsecure)s RESETPASSWORD #%(user.username)s <newpass> <newpass> %(resetcode)s" %obj

MAILTEMPLATES = {
  "mutators": {
    1: generate_url,
    2: generate_resetpassword,
    3: generate_resetcode,
    5: generate_resetcode,
    6: generate_activation_url,
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

Note that this URL will not work forever, you should make a note of your password
or change it (as on the site).

In case you forget your login/password use:
/msg %(config.bot)s REQUESTPASSWORD %(user.email)s

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
      2: { "subject": "%(config.bot)s reset password", "body": """
A password reset was requested for your account, to reset your password please use:
%(resetline)s

Where <newpass> should be replaced with your desired password.

For more information please visit the resetpassword help link at http://www.quakenet.org/help/q-commands/resetpassword

If it was not you who issued this command, please disregard this mail.
""", },
      3: { "subject": "%(config.bot)s password change", "body": """
Your password has recently changed. If this was not requested by you,
please use:
%(resetline)s

You have until %(lockuntil)s to perform this command.
""", },
      4: { "subject": "%(config.bot)s account reset", "body": """
Your %(config.bot)s account settings have been restored:
  E-mail address: %(user.email)s
  Password:       %(user.password)s

Make sure you read the %(config.bot)s security FAQ at %(config.securityurl)s.
""", },
      5: { "subject": "%(config.bot)s email change", "body": """
Your email address has been changed on %(config.bot)s from %(prevemail)s to %(user.email)s.

If you did not request this please use:
%(resetline)s

You have until %(lockuntil)s to perform this command.
""", },
      6: { "subject": "Please complete your QuakeNet account registration", "body": """
Hi %(user.username)s,

You're just one click away from completing your registration for a QuakeNet account!

Just visit the link below to complete the process:
%(url)s

For reference, your username/password is:

Username: %(user.username)s
Password: %(user.password)s

After activation you can auth yourself to QuakeNet by typing the following command:

   /AUTH %(user.username)s %(user.password)s
""", },
    },
  },
}
