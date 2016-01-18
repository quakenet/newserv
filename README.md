newserv
=======

[![Build Status](https://travis-ci.org/quakenet/newserv.png?branch=master)](https://travis-ci.org/quakenet/newserv)

Introduction
------------

newserv is a P10 protocol services daemon developed for the QuakeNet IRC
Network.

It is modular, fast and easily customisable.

The official website for newserv is https://development.quakenet.org/.

Please refer to the LICENSE file for licensing details.

Features
--------

 * Role-based access checks for oper commands
 * Search functionality
 * Scripting (Lua)
 * Proxy detection (P)
 * Connection limits ("trusts")
 * Transactional g:line support
 * Jupes
 * Chanfix
 * Chanserv (Q9)
 * Help Service (G)
 * Channel Service Request (R)
 * QA/Tutor Bots
 * Server list with latency checks

Requirements
------------

* IRC Server running the P10 Protocol. Full support for all modules may require use of QuakeNet's [snircd IRC server](https://development.quakenet.org), which is based upon [Undernet's ircu](http://coder-com.undernet.org/).
* Linux system (BSDs may work, but not actively tested)
* flex
* bison
* GNU Make
* Python 2.4

Support & Development
---------------------

Please read the documentation provided before you ask us for support. You may
find some assistance in #dev on QuakeNet for specific questions.

If you've found any bugs or you're working on any cool new features please give
us a shout.

Installation
------------

First run configure script:

    $ ./configure

The configure script will list any missing dependencies. If you're unsure why
a certain library or header file was not found you can run the configure script
with the -v option or check the .configure.log file after your first configure
run.

Please refer to the "Local Settings" section in this file if you're using
non-standard library/header search paths. Once you've resolved all dependency
issues you can build newserv:

    $ make

After all modules are built you can install newserv:

    $ make install

By default the newserv binary and the modules are installed into your source
tree. The recommended setup is to now create a separate directory and symlink
the "newserv" binary and the "modules" directory into it:

    $ cd
    $ mkdir newserv-install && cd newserv-install
    $ ln -s ../newserv-src/newserv
    $ ln -s ../newserv-src/modules

You will also need to copy the newserv.conf.example configuration file to your
installation directory and rename it to newserv.conf. The MODULES file has a
list of available modules and their configuration settings.

After you have updated your newserv.conf file you can start newserv:

    $ ./newserv

newserv does not detach from the console. Consider running it in a
screen(1) session.

User Accounts
-------------

You can create a user on your control instance using /msg N hello (where N is
the nick of your control user). You need to be opered and authed in order to
use this command.

If your network does not have an authentication service that supports account
IDs you can load the "auth" module. Note that this module lets opers set
arbitrary account names and IDs and therefore should probably not be loaded on
production networks.

Once you have an account you should have a look at /msg N showcommands for a
list of available commands.

Local Settings
--------------

If you are using non-standard library/include paths you can create a file
called configure.ini.local (using configure.ini.local.example as a template) to
override some of the settings.
