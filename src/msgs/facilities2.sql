/* MAX_NUMBER is the next number to be used, always one more than the highest message number. */
set bulk_insert INSERT INTO FACILITIES (LAST_CHANGE, FACILITY, FAC_CODE, MAX_NUMBER) VALUES (?, ?, ?, ?);
--
('2014-04-04 16:13:24', 'JRD', 0, 768)
('2012-01-23 20:10:30', 'QLI', 1, 532)
('2013-11-13 15:59:10', 'GFIX', 3, 122)
('1996-11-07 13:39:40', 'GPRE', 4, 1)
('2012-08-27 21:26:00', 'DSQL', 7, 33)
('2014-03-31 18:46:42', 'DYN', 8, 288)
('1996-11-07 13:39:40', 'INSTALL', 10, 1)
('1996-11-07 13:38:41', 'TEST', 11, 4)
('2014-04-04 16:08:08', 'GBAK', 12, 354)
('2014-04-04 16:36:53', 'SQLERR', 13, 1035)
('1996-11-07 13:38:42', 'SQLWARN', 14, 613)
('2006-09-10 03:04:31', 'JRD_BUGCHK', 15, 307)
('2014-01-13 15:41:16', 'ISQL', 17, 184)
('2010-07-10 10:50:30', 'GSEC', 18, 105)
('2012-05-25 19:59:42', 'GSTAT', 21, 56)
('2013-12-19 17:31:31', 'FBSVCMGR', 22, 58)
('2009-07-18 12:12:12', 'UTL', 23, 2)
('2011-05-25 16:17:34', 'NBACKUP', 24, 74)
('2009-07-20 07:55:48', 'FBTRACEMGR', 25, 41)
stop

COMMIT WORK;
