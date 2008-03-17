CREATE SCHEMA userdb;

CREATE TABLE userdb.languages (
  languageid       int8    NOT NULL,
  languagecode     VARCHAR NOT NULL,
  languagename     VARCHAR NOT NULL,
  replicationflags int4    NOT NULL,
  created          int8    NOT NULL,
  modified         int8    NOT NULL,
  revision         int8    NOT NULL,
  transactionid    int8    NOT NULL,
  PRIMARY KEY (languageid)) WITHOUT OIDS;

CREATE TABLE userdb.users (
  userid           int8    NOT NULL,
  username         VARCHAR NOT NULL,
  password         BYTEA   NOT NULL,
  languageid       int8    NOT NULL,
  emaillocal       VARCHAR NOT NULL,
  emaildomain      VARCHAR NOT NULL,
  statusflags      int4    NOT NULL,
  groupflags       int4    NOT NULL,
  optionflags      int4    NOT NULL,
  noticeflags      int4    NOT NULL,
  suspendstart     int8    NULL,
  suspendend       int8    NULL,
  suspendreason    VARCHAR NULL,
  suspenderid      int8    NULL,
  replicationflags int4    NOT NULL,
  created          int8    NOT NULL,
  modified         int8    NOT NULL,
  revision         int8    NOT NULL,
  transactionid    int8    NOT NULL,
  PRIMARY KEY (userid)) WITHOUT OIDS;

CREATE TABLE userdb.usertagsinfo (
  tagid            int4    NOT NULL,
  tagtype          int4    NOT NULL,
  tagname          VARCHAR NOT NULL,
  PRIMARY KEY (tagid)) WITHOUT OIDS;

CREATE TABLE userdb.usertagsdata (
  tagid            int4    NOT NULL,
  userid           int8    NOT NULL,
  tagdata          BYTEA   NOT NULL,
  PRIMARY KEY (tagid,userid)) WITHOUT OIDS;

CREATE TABLE userdb.maildomainconfigs (
  configid         int8    NOT NULL,
  configname       VARCHAR NOT NULL,
  domainflags      int4    NOT NULL,
  maxlocal         int8    NOT NULL,
  maxdomain        int8    NOT NULL,
  replicationflags int4    NOT NULL,
  created          int8    NOT NULL,
  modified         int8    NOT NULL,
  revision         int8    NOT NULL,
  transactionid    int8    NOT NULL,
  PRIMARY KEY (configid)) WITHOUT OIDS;

CREATE TABLE userdb.dbstate (
  variableid       int4    NOT NULL,
  variablevalue    int8    NOT NULL,
  PRIMARY KEY (variableid)) WITHOUT OIDS;

CREATE OR REPLACE FUNCTION userdb.resetdb() RETURNS void AS '
  UPDATE userdb.dbstate SET variablevalue = 0;
  TRUNCATE TABLE userdb.languages;
  TRUNCATE TABLE userdb.users;
  TRUNCATE TABLE userdb.usertagsinfo;
  TRUNCATE TABLE userdb.usertagsdata;
  TRUNCATE TABLE userdb.maildomainconfigs;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.miniresetdb() RETURNS void AS '
  UPDATE userdb.dbstate SET variablevalue = 0;
  DELETE FROM userdb.users WHERE userid != 1;
  UPDATE userdb.users SET revision = 1 WHERE userid = 1;
  UPDATE userdb.users SET transactionid = 1 WHERE userid = 1;
  UPDATE userdb.dbstate SET variablevalue = 1 WHERE variableid = 3;
  UPDATE userdb.dbstate SET variablevalue = 1 WHERE variableid = 4;
  TRUNCATE TABLE userdb.languages;
  TRUNCATE TABLE userdb.usertagsinfo;
  TRUNCATE TABLE userdb.usertagsdata;
  TRUNCATE TABLE userdb.maildomainconfigs;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.addlanguage(int8,varchar,varchar,int4,int8,int8,int8,int8) RETURNS void AS '
  INSERT INTO userdb.languages (languageid,languagecode,languagename,replicationflags,created,modified,revision,transactionid) VALUES ($1,$2,$3,$4,$5,$6,$7,$8);
  UPDATE userdb.dbstate SET variablevalue = $1 WHERE variableid = 1;
  UPDATE userdb.dbstate SET variablevalue = $8 WHERE variableid = 2;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.modifylanguage(int8,varchar,varchar,int4,int8,int8,int8,int8) RETURNS void AS '
  UPDATE userdb.languages SET languagecode = $2 , languagename = $3 , replicationflags = $4 , created = $5 , modified = $6, revision = $7 , transactionid = $8 WHERE languageid = $1;
  UPDATE userdb.dbstate SET variablevalue = $8 WHERE variableid = 2;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.deletelanguage(int8) RETURNS void AS '
  DELETE FROM userdb.languages WHERE languageid = $1;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.adduser(int8,varchar,bytea,int8,varchar,varchar,int4,int4,int4,int4,int8,int8,varchar,int8,int4,int8,int8,int8,int8) RETURNS void AS '
  INSERT INTO userdb.users (userid,username,password,languageid,emaillocal,emaildomain,statusflags,groupflags,optionflags,noticeflags,suspendstart,suspendend,suspendreason,suspenderid,replicationflags,created,modified,revision,transactionid) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16,$17,$18,$19);
  UPDATE userdb.dbstate SET variablevalue = $1 WHERE variableid = 3;
  UPDATE userdb.dbstate SET variablevalue = $19 WHERE variableid = 4;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.modifyuser(int8,varchar,bytea,int8,varchar,varchar,int4,int4,int4,int4,int8,int8,varchar,int8,int4,int8,int8,int8,int8) RETURNS void AS '
  UPDATE userdb.users SET username = $2 , password = $3, languageid = $4 , emaillocal = $5 , emaildomain = $6 , statusflags = $7 , groupflags = $8 , optionflags = $9 , noticeflags = $10 , suspendstart = $11 , suspendend = $12 , suspendreason = $13 , suspenderid = $14 , replicationflags = $15 , created = $16 , modified = $17, revision = $18 , transactionid = $19 WHERE userid = $1;
  UPDATE userdb.dbstate SET variablevalue = $19 WHERE variableid = 4;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.deleteuser(int8) RETURNS void AS '
  DELETE FROM userdb.users WHERE userid = $1;
  DELETE FROM userdb.usertagsdata WHERE userid = $1;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.addusertaginfo(int4,int4,varchar) RETURNS void AS '
  DELETE FROM userdb.usertagsdata WHERE tagid = $1;
  INSERT INTO userdb.usertagsinfo (tagid,tagtype,tagname) VALUES ($1,$2,$3);
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.deleteusertaginfo(int4) RETURNS void AS '
  DELETE FROM userdb.usertagsinfo WHERE tagid = $1;
  DELETE FROM userdb.usertagsdata WHERE tagid = $1;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.addusertagdata(int4,int8,bytea) RETURNS void AS '
  INSERT INTO userdb.usertagsdata (tagid,userid,tagdata) VALUES ($1,$2,$3);
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.modifyusertagdata(int4,int8,bytea) RETURNS void AS '
  UPDATE userdb.usertagsdata SET tagdata = $3 WHERE tagid = $1 AND userid = $2;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.deleteusertagdata(int4,int8) RETURNS void AS '
  DELETE FROM userdb.usertagsdata WHERE tagid = $1 AND userid = $2;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.deleteallusertagdatabytagid(int4) RETURNS void AS '
  DELETE FROM userdb.usertagsdata WHERE tagid = $1;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.deleteallusertagdatabyuserid(int8) RETURNS void AS '
  DELETE FROM userdb.usertagsdata WHERE userid = $1;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.addmaildomainconfig(int8,varchar,int4,int8,int8,int4,int8,int8,int8,int8) RETURNS void AS '
  INSERT INTO userdb.maildomainconfigs (configid,configname,domainflags,maxlocal,maxdomain,replicationflags,created,modified,revision,transactionid) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10);
  UPDATE userdb.dbstate SET variablevalue = $1 WHERE variableid = 5;
  UPDATE userdb.dbstate SET variablevalue = $10 WHERE variableid = 6;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.modifymaildomainconfig(int8,varchar,int4,int8,int8,int4,int8,int8,int8,int8) RETURNS void AS '
  UPDATE userdb.maildomainconfigs SET configname = $2 , domainflags = $3 , maxlocal = $4 , maxdomain = $5 , replicationflags = $6 , created = $7 , modified = $8 , revision = $9 , transactionid = $10 WHERE configid = $1;
  UPDATE userdb.dbstate SET variablevalue = $10 WHERE variableid = 6;
' LANGUAGE SQL;

CREATE OR REPLACE FUNCTION userdb.deletemaildomainconfig(int8) RETURNS void AS '
  DELETE FROM userdb.maildomainconfigs WHERE configid = $1;
' LANGUAGE SQL;

INSERT INTO userdb.dbstate (variableid,variablevalue) VALUES (1,0);
INSERT INTO userdb.dbstate (variableid,variablevalue) VALUES (2,0);
INSERT INTO userdb.dbstate (variableid,variablevalue) VALUES (3,0);
INSERT INTO userdb.dbstate (variableid,variablevalue) VALUES (4,0);
INSERT INTO userdb.dbstate (variableid,variablevalue) VALUES (5,0);
INSERT INTO userdb.dbstate (variableid,variablevalue) VALUES (6,0);
