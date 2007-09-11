BEGIN;



--
-- recordinglocation
--

INSERT INTO recordinglocation VALUES (10, 'Den');

SELECT setval('rlc_id_seq', max(rlc_identifier)) FROM recordinglocation;


--
-- sourceconfig
--

INSERT INTO sourceconfig VALUES (100, 'Den-VT1', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (101, 'Den-VT2', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (102, 'Den-VT3', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (103, 'Den-VT4', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (104, 'Den-VT5', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (105, 'Den-VT6', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (106, 'Den-VT7', 2, NULL, 10);

SELECT setval('scf_id_seq', max(scf_identifier)) FROM sourceconfig;


--
-- sourcetrackconfig
--

-- Den
INSERT INTO sourcetrackconfig VALUES (200, 1, 1, 'V',  1, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (201, 2, 1, 'A1', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (202, 3, 2, 'A2', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (203, 4, 3, 'A3', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (204, 5, 4, 'A4', 2, '(25,1)', 10800000, 100);
INSERT INTO sourcetrackconfig VALUES (210, 1, 1, 'V',  1, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (211, 2, 1, 'A1', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (212, 3, 2, 'A2', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (213, 4, 3, 'A3', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (214, 5, 4, 'A4', 2, '(25,1)', 10800000, 101);
INSERT INTO sourcetrackconfig VALUES (220, 1, 1, 'V',  1, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (221, 2, 1, 'A1', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (222, 3, 2, 'A2', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (223, 4, 3, 'A3', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (224, 5, 4, 'A4', 2, '(25,1)', 10800000, 102);
INSERT INTO sourcetrackconfig VALUES (230, 1, 1, 'V',  1, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (231, 2, 1, 'A1', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (232, 3, 2, 'A2', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (233, 4, 3, 'A3', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (234, 5, 4, 'A4', 2, '(25,1)', 10800000, 103);
INSERT INTO sourcetrackconfig VALUES (240, 1, 1, 'V',  1, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (241, 2, 1, 'A1', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (242, 3, 2, 'A2', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (243, 4, 3, 'A3', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (244, 5, 4, 'A4', 2, '(25,1)', 10800000, 104);
INSERT INTO sourcetrackconfig VALUES (250, 1, 1, 'V',  1, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (251, 2, 1, 'A1', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (252, 3, 2, 'A2', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (253, 4, 3, 'A3', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (254, 5, 4, 'A4', 2, '(25,1)', 10800000, 105);
INSERT INTO sourcetrackconfig VALUES (260, 1, 1, 'V',  1, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (261, 2, 1, 'A1', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (262, 3, 2, 'A2', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (263, 4, 3, 'A3', 2, '(25,1)', 10800000, 106);
INSERT INTO sourcetrackconfig VALUES (264, 5, 4, 'A4', 2, '(25,1)', 10800000, 106);


SELECT setval('sct_id_seq', max(sct_identifier)) FROM sourcetrackconfig;


--
-- recorder
--

INSERT INTO recorder VALUES (1, 'Ingex1', NULL);
INSERT INTO recorder VALUES (2, 'Ingex2', NULL);

SELECT setval('rer_id_seq', max(rer_identifier)) FROM recorder;


--
-- recorderconfig
--

INSERT INTO recorderconfig VALUES (10, 'Den1', 1);
INSERT INTO recorderconfig VALUES (11, 'Den2', 2);

SELECT setval('rec_id_seq', max(rec_identifier)) FROM recorderconfig;



--
-- defaultrecorderparameters
--

INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (1, 'MXF', 'true', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (2, 'RAW', 'false', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (3, 'QUAD_MPEG2', 'false', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (4, 'BROWSE_AUDIO', 'true', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (5, 'IMAGE_ASPECT', '16/9', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (6, 'MXF_RESOLUTION', '9', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (7, 'MPEG2_BITRATE', '5000', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (8, 'RAW_AUDIO_BITS', '32', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (9, 'MXF_AUDIO_BITS', '16', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (10, 'BROWSE_AUDIO_BITS', '16', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (11, 'RAW_DIR', '/video/raw/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (12, 'MXF_DIR', '/video/mxf/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (13, 'MXF_SUBDIR_CREATING', 'Creating/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (14, 'MXF_SUBDIR_FAILURES', 'Failures/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (15, 'BROWSE_DIR', '/video/browse/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (16, 'COPY_COMMAND', '', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (17, 'TIMECODE_MODE', '2', 1);
    
--
-- recorderparameters
--

-- Ingex1 - Den
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '9', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '2', 1, 10);

-- Ingex2 - Den
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '9', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '2', 1, 11);



--
-- recorder update
--

UPDATE recorder SET rer_conf_id=10 WHERE rer_identifier = 1;
UPDATE recorder SET rer_conf_id=11 WHERE rer_identifier = 2;


--
-- recorderinputconfig
--

-- Ingex1
-- Den
INSERT INTO recorderinputconfig VALUES (100, 1, 'Card 0', 10);
INSERT INTO recorderinputconfig VALUES (101, 2, 'Card 1', 10);
INSERT INTO recorderinputconfig VALUES (102, 3, 'Card 2', 10);
INSERT INTO recorderinputconfig VALUES (103, 4, 'Card 3', 10);

-- Ingex2
-- Den
INSERT INTO recorderinputconfig VALUES (110, 1, 'Card 0', 11);
INSERT INTO recorderinputconfig VALUES (111, 2, 'Card 1', 11);
INSERT INTO recorderinputconfig VALUES (112, 3, 'Card 2', 11);


SELECT setval('ric_id_seq', max(ric_identifier)) FROM recorderinputconfig;


--
-- recorderinputtrackconfig
--

-- Ingex1 Den
INSERT INTO recorderinputtrackconfig VALUES (400, 1, 1, 100, 100, 1);
INSERT INTO recorderinputtrackconfig VALUES (401, 2, 1, 100, 100, 2);
INSERT INTO recorderinputtrackconfig VALUES (402, 3, 2, 100, 100, 3);
INSERT INTO recorderinputtrackconfig VALUES (403, 4, 0, 100, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (404, 5, 0, 100, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (410, 1, 1, 101, 101, 1);
INSERT INTO recorderinputtrackconfig VALUES (411, 2, 1, 101, 101, 2);
INSERT INTO recorderinputtrackconfig VALUES (412, 3, 2, 101, 101, 3);
INSERT INTO recorderinputtrackconfig VALUES (413, 4, 0, 101, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (414, 5, 0, 101, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (420, 1, 1, 102, 102, 1);
INSERT INTO recorderinputtrackconfig VALUES (421, 2, 1, 102, 102, 2);
INSERT INTO recorderinputtrackconfig VALUES (422, 3, 2, 102, 102, 3);
INSERT INTO recorderinputtrackconfig VALUES (423, 4, 0, 102, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (424, 5, 0, 102, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (430, 1, 1, 103, 103, 1);
INSERT INTO recorderinputtrackconfig VALUES (431, 2, 1, 103, 103, 2);
INSERT INTO recorderinputtrackconfig VALUES (432, 3, 2, 103, 103, 3);
INSERT INTO recorderinputtrackconfig VALUES (433, 4, 0, 103, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (434, 5, 0, 103, NULL, NULL);


-- Ingex2 Den
INSERT INTO recorderinputtrackconfig VALUES (500, 1, 1, 110, 104, 1);
INSERT INTO recorderinputtrackconfig VALUES (501, 2, 1, 110, 104, 2);
INSERT INTO recorderinputtrackconfig VALUES (502, 3, 2, 110, 104, 3);
INSERT INTO recorderinputtrackconfig VALUES (503, 4, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (504, 5, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (510, 1, 1, 111, 105, 1);
INSERT INTO recorderinputtrackconfig VALUES (511, 2, 1, 111, 105, 2);
INSERT INTO recorderinputtrackconfig VALUES (512, 3, 2, 111, 105, 3);
INSERT INTO recorderinputtrackconfig VALUES (513, 4, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (514, 5, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (520, 1, 1, 112, 106, 1);
INSERT INTO recorderinputtrackconfig VALUES (521, 2, 1, 112, 106, 2);
INSERT INTO recorderinputtrackconfig VALUES (522, 3, 2, 112, 106, 3);
INSERT INTO recorderinputtrackconfig VALUES (523, 4, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (524, 5, 0, 112, NULL, NULL);


SELECT setval('rtc_id_seq', max(rtc_identifier)) FROM recorderinputtrackconfig;


--
-- multicameraclipdef
--

INSERT INTO multicameraclipdef VALUES (1, 'Den 5 way multi-camera clip');

SELECT setval('mcd_id_seq', max(mcd_identifier)) FROM multicameraclipdef;


--
-- multicameratrackdef
--

-- Den 5 way multi-camera clip
-- one video, two audio
INSERT INTO multicameratrackdef VALUES (1, 1, 1, 1);
INSERT INTO multicameratrackdef VALUES (2, 2, 1, 1);
INSERT INTO multicameratrackdef VALUES (3, 3, 2, 1);

SELECT setval('mct_id_seq', max(mct_identifier)) FROM multicameratrackdef;


--
-- multicameraselectordef
--

-- Den
-- 5 selectors for the video track, 1 selector for each audio track
INSERT INTO multicameraselectordef VALUES (100, 1, 1, 100, 1);
INSERT INTO multicameraselectordef VALUES (101, 2, 1, 101, 1);
INSERT INTO multicameraselectordef VALUES (102, 3, 1, 102, 1);
INSERT INTO multicameraselectordef VALUES (103, 4, 1, 103, 1);
INSERT INTO multicameraselectordef VALUES (104, 5, 1, 104, 1);
INSERT INTO multicameraselectordef VALUES (105, 1, 2, 100, 2);
INSERT INTO multicameraselectordef VALUES (106, 1, 3, 100, 3);

SELECT setval('mcs_id_seq', max(mcs_identifier)) FROM multicameraselectordef;




--
-- Proxy definitions
---




END;

