BEGIN;



--
-- recordinglocation
--

INSERT INTO recordinglocation VALUES (10, 'Studio');

SELECT setval('rlc_id_seq', max(rlc_identifier)) FROM recordinglocation;


--
-- sourceconfig
--

INSERT INTO sourceconfig VALUES (100, 'Cam1', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (101, 'Cam2', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (102, 'Cam3', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (103, 'Cam4', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (104, 'Cam5', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (105, 'Cam6', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (106, 'Cam7', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (107, 'Cam8', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (110, 'Mixer Out', 2, NULL, 10);

SELECT setval('scf_id_seq', max(scf_identifier)) FROM sourceconfig;


--
-- sourcetrackconfig
--

-- Cam1
INSERT INTO sourcetrackconfig VALUES (200, 1, 1, 'V',  1, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (201, 2, 1, 'A1', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (202, 3, 2, 'A2', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (203, 4, 3, 'A3', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (204, 5, 4, 'A4', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (205, 6, 5, 'A5', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (206, 7, 6, 'A6', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (207, 8, 7, 'A7', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (208, 9, 8, 'A8', 2, '(25,1)', 10800000, 100);
-- Cam2
INSERT INTO sourcetrackconfig VALUES (210, 1, 1, 'V',  1, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (211, 2, 1, 'A1', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (212, 3, 2, 'A2', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (213, 4, 3, 'A3', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (214, 5, 4, 'A4', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (215, 6, 5, 'A5', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (216, 7, 6, 'A6', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (217, 8, 7, 'A7', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (218, 9, 8, 'A8', 2, '(25,1)', 10800000, 101);
-- Cam3
INSERT INTO sourcetrackconfig VALUES (220, 1, 1, 'V',  1, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (221, 2, 1, 'A1', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (222, 3, 2, 'A2', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (223, 4, 3, 'A3', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (224, 5, 4, 'A4', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (225, 6, 5, 'A5', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (226, 7, 6, 'A6', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (227, 8, 7, 'A7', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (228, 9, 8, 'A8', 2, '(25,1)', 10800000, 102);
-- Cam4
INSERT INTO sourcetrackconfig VALUES (230, 1, 1, 'V',  1, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (231, 2, 1, 'A1', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (232, 3, 2, 'A2', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (233, 4, 3, 'A3', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (234, 5, 4, 'A4', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (235, 6, 5, 'A5', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (236, 7, 6, 'A6', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (237, 8, 7, 'A7', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (238, 9, 8, 'A8', 2, '(25,1)', 10800000, 103);
-- Cam5
INSERT INTO sourcetrackconfig VALUES (240, 1, 1, 'V',  1, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (241, 2, 1, 'A1', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (242, 3, 2, 'A2', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (243, 4, 3, 'A3', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (244, 5, 4, 'A4', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (245, 6, 5, 'A5', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (246, 7, 6, 'A6', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (247, 8, 7, 'A7', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (248, 9, 8, 'A8', 2, '(25,1)', 10800000, 104);
-- Cam6
INSERT INTO sourcetrackconfig VALUES (250, 1, 1, 'V',  1, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (251, 2, 1, 'A1', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (252, 3, 2, 'A2', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (253, 4, 3, 'A3', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (254, 5, 4, 'A4', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (255, 6, 5, 'A5', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (256, 7, 6, 'A6', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (257, 8, 7, 'A7', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (258, 9, 8, 'A8', 2, '(25,1)', 10800000, 105);
-- Cam7
INSERT INTO sourcetrackconfig VALUES (260, 1, 1, 'V',  1, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (261, 2, 1, 'A1', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (262, 3, 2, 'A2', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (263, 4, 3, 'A3', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (264, 5, 4, 'A4', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (265, 6, 5, 'A5', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (266, 7, 6, 'A6', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (267, 8, 7, 'A7', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (268, 9, 8, 'A8', 2, '(25,1)', 10800000, 106);
-- Cam8
INSERT INTO sourcetrackconfig VALUES (270, 1, 1, 'V',  1, '(25,1)', 10800000, 107);
INSERT INTO sourcetrackconfig VALUES (271, 2, 1, 'A1', 2, '(25,1)', 10800000, 107);
INSERT INTO sourcetrackconfig VALUES (272, 3, 2, 'A2', 2, '(25,1)', 10800000, 107);
INSERT INTO sourcetrackconfig VALUES (273, 4, 3, 'A3', 2, '(25,1)', 10800000, 107);
INSERT INTO sourcetrackconfig VALUES (274, 5, 4, 'A4', 2, '(25,1)', 10800000, 107);
INSERT INTO sourcetrackconfig VALUES (275, 6, 5, 'A5', 2, '(25,1)', 10800000, 107);
INSERT INTO sourcetrackconfig VALUES (276, 7, 6, 'A6', 2, '(25,1)', 10800000, 107);
INSERT INTO sourcetrackconfig VALUES (277, 8, 7, 'A7', 2, '(25,1)', 10800000, 107);
INSERT INTO sourcetrackconfig VALUES (278, 9, 8, 'A8', 2, '(25,1)', 10800000, 107);
-- Mixer Out
INSERT INTO sourcetrackconfig VALUES (300, 1, 1, 'V',  1, '(25,1)', 10800000, 110);


SELECT setval('sct_id_seq', max(sct_identifier)) FROM sourcetrackconfig;


--
-- multicameraclipdef
--

INSERT INTO multicameraclipdef VALUES (10, 'Studio multicam');

SELECT setval('mcd_id_seq', max(mcd_identifier)) FROM multicameraclipdef;


--
-- multicameratrackdef
--

-- multi-camera tracks
-- mct_identifier | mct_index | mct_track_number | mct_multi_camera_def_id
INSERT INTO multicameratrackdef VALUES (1, 1, 1, 10);
INSERT INTO multicameratrackdef VALUES (2, 2, 1, 10);
INSERT INTO multicameratrackdef VALUES (3, 3, 2, 10);
INSERT INTO multicameratrackdef VALUES (4, 4, 3, 10);
INSERT INTO multicameratrackdef VALUES (5, 5, 4, 10);
INSERT INTO multicameratrackdef VALUES (6, 6, 5, 10);
INSERT INTO multicameratrackdef VALUES (7, 7, 6, 10);
INSERT INTO multicameratrackdef VALUES (8, 8, 7, 10);
INSERT INTO multicameratrackdef VALUES (9, 9, 8, 10);

SELECT setval('mct_id_seq', max(mct_identifier)) FROM multicameratrackdef;


--
-- multicameraselectordef
-- mcs_identifier | mcs_index | mcs_multi_camera_track_def_id | mcs_source_id | mcs_source_track_id
--

INSERT INTO multicameraselectordef VALUES (101, 1, 1, 100, 1);
INSERT INTO multicameraselectordef VALUES (102, 2, 1, 101, 1);
INSERT INTO multicameraselectordef VALUES (103, 3, 1, 102, 1);
INSERT INTO multicameraselectordef VALUES (104, 4, 1, 103, 1);
INSERT INTO multicameraselectordef VALUES (201, 1, 2, 100, 2);
INSERT INTO multicameraselectordef VALUES (202, 2, 2, 101, 2);
INSERT INTO multicameraselectordef VALUES (203, 3, 2, 102, 2);
INSERT INTO multicameraselectordef VALUES (204, 4, 2, 103, 2);
INSERT INTO multicameraselectordef VALUES (301, 1, 3, 100, 3);
INSERT INTO multicameraselectordef VALUES (302, 2, 3, 101, 3);
INSERT INTO multicameraselectordef VALUES (303, 3, 3, 102, 3);
INSERT INTO multicameraselectordef VALUES (304, 4, 3, 103, 3);
INSERT INTO multicameraselectordef VALUES (401, 1, 4, 100, 4);
INSERT INTO multicameraselectordef VALUES (402, 2, 4, 101, 4);
INSERT INTO multicameraselectordef VALUES (403, 3, 4, 102, 4);
INSERT INTO multicameraselectordef VALUES (404, 4, 4, 103, 4);
INSERT INTO multicameraselectordef VALUES (501, 1, 5, 100, 5);
INSERT INTO multicameraselectordef VALUES (502, 2, 5, 101, 5);
INSERT INTO multicameraselectordef VALUES (503, 3, 5, 102, 5);
INSERT INTO multicameraselectordef VALUES (504, 4, 5, 103, 5);
INSERT INTO multicameraselectordef VALUES (601, 1, 6, 100, 6);
INSERT INTO multicameraselectordef VALUES (602, 2, 6, 101, 6);
INSERT INTO multicameraselectordef VALUES (603, 3, 6, 102, 6);
INSERT INTO multicameraselectordef VALUES (604, 4, 6, 103, 6);
INSERT INTO multicameraselectordef VALUES (701, 1, 7, 100, 7);
INSERT INTO multicameraselectordef VALUES (702, 2, 7, 101, 7);
INSERT INTO multicameraselectordef VALUES (703, 3, 7, 102, 7);
INSERT INTO multicameraselectordef VALUES (704, 4, 7, 103, 7);
INSERT INTO multicameraselectordef VALUES (801, 1, 8, 100, 8);
INSERT INTO multicameraselectordef VALUES (802, 2, 8, 101, 8);
INSERT INTO multicameraselectordef VALUES (803, 3, 8, 102, 8);
INSERT INTO multicameraselectordef VALUES (804, 4, 8, 103, 8);
INSERT INTO multicameraselectordef VALUES (901, 1, 9, 100, 9);
INSERT INTO multicameraselectordef VALUES (902, 2, 9, 101, 9);
INSERT INTO multicameraselectordef VALUES (903, 3, 9, 102, 9);
INSERT INTO multicameraselectordef VALUES (904, 4, 9, 103, 9);

SELECT setval('mcs_id_seq', max(mcs_identifier)) FROM multicameraselectordef;


--
-- recorder
--

INSERT INTO recorder VALUES (1, 'Ingex');
INSERT INTO recorder VALUES (2, 'Ingex-HD');
INSERT INTO recorder VALUES (3, 'Ingex-HD2');
INSERT INTO recorder VALUES (9, 'Ingex-Router');

SELECT setval('rer_id_seq', max(rer_identifier)) FROM recorder;


--
-- recorderconfig
--

INSERT INTO recorderconfig VALUES (10, 'SD-MXF');
INSERT INTO recorderconfig VALUES (11, 'SD-Quicktime');
INSERT INTO recorderconfig VALUES (20, 'HD-MXF');
INSERT INTO recorderconfig VALUES (21, 'HD-Quicktime');
INSERT INTO recorderconfig VALUES (90, 'Router');

SELECT setval('rec_id_seq', max(rec_identifier)) FROM recorderconfig;


--
-- recorder current config
--

UPDATE recorder SET rer_conf_id=10 WHERE rer_identifier = 1;
UPDATE recorder SET rer_conf_id=20 WHERE rer_identifier = 2;
UPDATE recorder SET rer_conf_id=20 WHERE rer_identifier = 3;
UPDATE recorder SET rer_conf_id=90 WHERE rer_identifier = 9;

--
-- recorderinputconfig
--

-- Ingex
INSERT INTO recorderinputconfig VALUES (100, 1, 'Input 0', 1);
INSERT INTO recorderinputconfig VALUES (101, 2, 'Input 1', 1);
INSERT INTO recorderinputconfig VALUES (102, 3, 'Input 2', 1);
INSERT INTO recorderinputconfig VALUES (103, 4, 'Input 3', 1);

-- Ingex-HD
INSERT INTO recorderinputconfig VALUES (200, 1, 'Input 0', 2);
INSERT INTO recorderinputconfig VALUES (201, 2, 'Input 1', 2);
INSERT INTO recorderinputconfig VALUES (202, 3, 'Input 2', 2);
INSERT INTO recorderinputconfig VALUES (203, 4, 'Input 3', 2);

-- Ingex-HD2
INSERT INTO recorderinputconfig VALUES (300, 1, 'Input 0', 3);
INSERT INTO recorderinputconfig VALUES (301, 2, 'Input 1', 3);

-- Ingex-Router
INSERT INTO recorderinputconfig VALUES (900, 1, 'Input 0', 9);


SELECT setval('ric_id_seq', max(ric_identifier)) FROM recorderinputconfig;


--
-- recorderinputtrackconfig
-- rtc_identifier | rtc_index | rtc_track_number | rtc_recorder_input_id | rtc_source_id | rtc_source_track_id
--

-- Ingex
INSERT INTO recorderinputtrackconfig VALUES (100, 1, 1, 100, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (101, 2, 1, 100, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (102, 3, 2, 100, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (103, 4, 3, 100, 100, 4);
INSERT INTO recorderinputtrackconfig VALUES (104, 5, 4, 100, 100, 5);
INSERT INTO recorderinputtrackconfig VALUES (105, 6, 5, 100, 100, 6);
INSERT INTO recorderinputtrackconfig VALUES (106, 7, 6, 100, 100, 7);
INSERT INTO recorderinputtrackconfig VALUES (107, 8, 7, 100, 100, 8);
INSERT INTO recorderinputtrackconfig VALUES (108, 9, 8, 100, 100, 9);

INSERT INTO recorderinputtrackconfig VALUES (110, 1, 1, 101, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (111, 2, 1, 101, 101, 2);
INSERT INTO recorderinputtrackconfig VALUES (112, 3, 2, 101, 101, 3);
INSERT INTO recorderinputtrackconfig VALUES (113, 4, 3, 101, 101, 4);
INSERT INTO recorderinputtrackconfig VALUES (114, 5, 4, 101, 101, 5);
INSERT INTO recorderinputtrackconfig VALUES (115, 6, 5, 101, 101, 6);
INSERT INTO recorderinputtrackconfig VALUES (116, 7, 6, 101, 101, 7);
INSERT INTO recorderinputtrackconfig VALUES (117, 8, 7, 101, 101, 8);
INSERT INTO recorderinputtrackconfig VALUES (118, 9, 8, 101, 101, 9);

INSERT INTO recorderinputtrackconfig VALUES (120, 1, 1, 102, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (121, 2, 1, 102, 102, 2);
INSERT INTO recorderinputtrackconfig VALUES (122, 3, 2, 102, 102, 3);
INSERT INTO recorderinputtrackconfig VALUES (123, 4, 3, 102, 102, 4);
INSERT INTO recorderinputtrackconfig VALUES (124, 5, 4, 102, 102, 5);
INSERT INTO recorderinputtrackconfig VALUES (125, 6, 5, 102, 102, 6);
INSERT INTO recorderinputtrackconfig VALUES (126, 7, 6, 102, 102, 7);
INSERT INTO recorderinputtrackconfig VALUES (127, 8, 7, 102, 102, 8);
INSERT INTO recorderinputtrackconfig VALUES (128, 9, 8, 102, 102, 9);

INSERT INTO recorderinputtrackconfig VALUES (130, 1, 1, 103, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (131, 2, 1, 103, 103, 2);
INSERT INTO recorderinputtrackconfig VALUES (132, 3, 2, 103, 103, 3);
INSERT INTO recorderinputtrackconfig VALUES (133, 4, 3, 103, 103, 4);
INSERT INTO recorderinputtrackconfig VALUES (134, 5, 4, 103, 103, 5);
INSERT INTO recorderinputtrackconfig VALUES (135, 6, 5, 103, 103, 6);
INSERT INTO recorderinputtrackconfig VALUES (136, 7, 6, 103, 103, 7);
INSERT INTO recorderinputtrackconfig VALUES (137, 8, 7, 103, 103, 8);
INSERT INTO recorderinputtrackconfig VALUES (138, 9, 8, 103, 103, 9);

-- Ingex-HD
INSERT INTO recorderinputtrackconfig VALUES (400, 1, 1, 200, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (401, 2, 1, 200, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (402, 3, 2, 200, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (403, 4, 3, 200, 100, 4);
INSERT INTO recorderinputtrackconfig VALUES (404, 5, 4, 200, 100, 5);
INSERT INTO recorderinputtrackconfig VALUES (405, 6, 5, 200, 100, 6);
INSERT INTO recorderinputtrackconfig VALUES (406, 7, 6, 200, 100, 7);
INSERT INTO recorderinputtrackconfig VALUES (407, 8, 7, 200, 100, 8);
INSERT INTO recorderinputtrackconfig VALUES (408, 9, 8, 200, 100, 9);

INSERT INTO recorderinputtrackconfig VALUES (410, 1, 1, 201, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (411, 2, 1, 201, 101, 2);
INSERT INTO recorderinputtrackconfig VALUES (412, 3, 2, 201, 101, 3);
INSERT INTO recorderinputtrackconfig VALUES (413, 4, 3, 201, 101, 4);
INSERT INTO recorderinputtrackconfig VALUES (414, 5, 4, 201, 101, 5);
INSERT INTO recorderinputtrackconfig VALUES (415, 6, 5, 201, 101, 6);
INSERT INTO recorderinputtrackconfig VALUES (416, 7, 6, 201, 101, 7);
INSERT INTO recorderinputtrackconfig VALUES (417, 8, 7, 201, 101, 8);
INSERT INTO recorderinputtrackconfig VALUES (418, 9, 8, 201, 101, 9);

INSERT INTO recorderinputtrackconfig VALUES (420, 1, 1, 202, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (421, 2, 1, 202, 102, 2);
INSERT INTO recorderinputtrackconfig VALUES (422, 3, 2, 202, 102, 3);
INSERT INTO recorderinputtrackconfig VALUES (423, 4, 3, 202, 102, 4);
INSERT INTO recorderinputtrackconfig VALUES (424, 5, 4, 202, 102, 5);
INSERT INTO recorderinputtrackconfig VALUES (425, 6, 5, 202, 102, 6);
INSERT INTO recorderinputtrackconfig VALUES (426, 7, 6, 202, 102, 7);
INSERT INTO recorderinputtrackconfig VALUES (427, 8, 7, 202, 102, 8);
INSERT INTO recorderinputtrackconfig VALUES (428, 9, 8, 202, 102, 9);

INSERT INTO recorderinputtrackconfig VALUES (430, 1, 1, 203, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (431, 2, 1, 203, 103, 2);
INSERT INTO recorderinputtrackconfig VALUES (432, 3, 2, 203, 103, 3);
INSERT INTO recorderinputtrackconfig VALUES (433, 4, 3, 203, 103, 4);
INSERT INTO recorderinputtrackconfig VALUES (434, 5, 4, 203, 103, 5);
INSERT INTO recorderinputtrackconfig VALUES (435, 6, 5, 203, 103, 6);
INSERT INTO recorderinputtrackconfig VALUES (436, 7, 6, 203, 103, 7);
INSERT INTO recorderinputtrackconfig VALUES (437, 8, 7, 203, 103, 8);
INSERT INTO recorderinputtrackconfig VALUES (438, 9, 8, 203, 103, 9);

-- Ingex-HD2
INSERT INTO recorderinputtrackconfig VALUES (700, 1, 1, 300, 104, 1);
INSERT INTO recorderinputtrackconfig VALUES (701, 2, 1, 300, 104, 2);
INSERT INTO recorderinputtrackconfig VALUES (702, 3, 2, 300, 104, 3);
INSERT INTO recorderinputtrackconfig VALUES (703, 4, 3, 300, 104, 4);
INSERT INTO recorderinputtrackconfig VALUES (704, 5, 4, 300, 104, 5);
INSERT INTO recorderinputtrackconfig VALUES (705, 6, 5, 300, 104, 6);
INSERT INTO recorderinputtrackconfig VALUES (706, 7, 6, 300, 104, 7);
INSERT INTO recorderinputtrackconfig VALUES (707, 8, 7, 300, 104, 8);
INSERT INTO recorderinputtrackconfig VALUES (708, 9, 8, 300, 104, 9);

INSERT INTO recorderinputtrackconfig VALUES (710, 1, 1, 301, 105, 1);
INSERT INTO recorderinputtrackconfig VALUES (711, 2, 1, 301, 105, 2);
INSERT INTO recorderinputtrackconfig VALUES (712, 3, 2, 301, 105, 3);
INSERT INTO recorderinputtrackconfig VALUES (713, 4, 3, 301, 105, 4);
INSERT INTO recorderinputtrackconfig VALUES (714, 5, 4, 301, 105, 5);
INSERT INTO recorderinputtrackconfig VALUES (715, 6, 5, 301, 105, 6);
INSERT INTO recorderinputtrackconfig VALUES (716, 7, 6, 301, 105, 7);
INSERT INTO recorderinputtrackconfig VALUES (717, 8, 7, 301, 105, 8);
INSERT INTO recorderinputtrackconfig VALUES (718, 9, 8, 301, 105, 9);

-- Ingex-Router
INSERT INTO recorderinputtrackconfig VALUES (900, 1, 1, 900, 110, 1);


SELECT setval('rtc_id_seq', max(rtc_identifier)) FROM recorderinputtrackconfig;



--
-- MXF SD online/offline plus browse
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '50', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_DEST', '/store/mxf_online/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_PRIORITY', '3', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '60', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_offline/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_DEST', '/store/mxf_offline/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_PRIORITY', '2', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_RESOLUTION', '210', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_BITC', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_DIR', '/video/browse/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_DEST', '/store/browse/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_PRIORITY', '1', 1, 10);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_DEST', '', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_PRIORITY', '3', 1, 10);

--
-- Quicktime DV50
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '34', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/dv/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_DEST', '/store/dv/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_PRIORITY', '1', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '0', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/dv/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_DEST', '/store/dv/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_PRIORITY', '2', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_RESOLUTION', '0', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_BITC', 'false', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_DIR', '/video/browse/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_DEST', '/store/browse/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_PRIORITY', '1', 1, 11);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_DEST', '', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_PRIORITY', '3', 1, 11);

--
-- MXF HD online SD offline
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '104', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/mxf_online/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_DEST', '/store/mxf_online/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_PRIORITY', '3', 1, 20);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '60', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/mxf_offline/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_DEST', '/store/mxf_offline/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_PRIORITY', '2', 1, 20);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_RESOLUTION', '0', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_BITC', 'false', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_DIR', '/video/browse/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_DEST', '/store/browse/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_PRIORITY', '1', 1, 20);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_DEST', '', 1, 20);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_PRIORITY', '3', 1, 20);

--
-- HD Quicktime DVCPRO-HD
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '44', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_BITC', 'false', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/dv/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_DEST', '/store/dv/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_PRIORITY', '1', 1, 21);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_RESOLUTION', '0', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_BITC', 'false', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_DIR', '/video/dv/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_DEST', '/store/dv/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE2_COPY_PRIORITY', '2', 1, 21);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_RESOLUTION', '0', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_BITC', 'false', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_DIR', '/video/browse/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_DEST', '/store/browse/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE3_COPY_PRIORITY', '1', 1, 21);
    
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_RESOLUTION', '0', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_BITC', 'true', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_DIR', '/video/browse/', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_DEST', '', 1, 21);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_COPY_PRIORITY', '3', 1, 21);

--
-- Router
--

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_RESOLUTION', '400', 1, 90);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_DIR', '/video/cuts/', 1, 90);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('ENCODE1_COPY_DEST', '/store/cuts/', 1, 90);



END;

