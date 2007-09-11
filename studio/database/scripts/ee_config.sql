BEGIN;



--
-- recordinglocation
--

INSERT INTO recordinglocation VALUES (10, 'Lot');
INSERT INTO recordinglocation VALUES (11, 'Stage');
INSERT INTO recordinglocation VALUES (12, 'Studio A');

SELECT setval('rlc_id_seq', max(rlc_identifier)) FROM recordinglocation;


--
-- sourceconfig
--

INSERT INTO sourceconfig VALUES (100, 'Lot - VT1', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (101, 'Lot - VT2', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (102, 'Lot - VT3', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (103, 'Lot - VT4', 2, NULL, 10);
INSERT INTO sourceconfig VALUES (110, 'Stage - VT1', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (111, 'Stage - VT2', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (112, 'Stage - VT3', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (113, 'Stage - VT4', 2, NULL, 11);
INSERT INTO sourceconfig VALUES (120, 'Studio A - VT1', 2, NULL, 12);
INSERT INTO sourceconfig VALUES (121, 'Studio A - VT2', 2, NULL, 12);
INSERT INTO sourceconfig VALUES (122, 'Studio A - VT3', 2, NULL, 12);
INSERT INTO sourceconfig VALUES (123, 'Studio A - VT4', 2, NULL, 12);

SELECT setval('scf_id_seq', max(scf_identifier)) FROM sourceconfig;


--
-- sourcetrackconfig
--

-- Simple Lot
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

-- Simple Stage
INSERT INTO sourcetrackconfig VALUES (300, 1, 1, 'V',  1, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (301, 2, 1, 'A1', 2, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (302, 3, 2, 'A2', 2, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (303, 4, 3, 'A3', 2, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (304, 5, 4, 'A4', 2, '(25,1)', 10800000, 110);
INSERT INTO sourcetrackconfig VALUES (310, 1, 1, 'V',  1, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (311, 2, 1, 'A1', 2, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (312, 3, 2, 'A2', 2, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (313, 4, 3, 'A3', 2, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (314, 5, 4, 'A4', 2, '(25,1)', 10800000, 111);
INSERT INTO sourcetrackconfig VALUES (320, 1, 1, 'V',  1, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (321, 2, 1, 'A1', 2, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (322, 3, 2, 'A2', 2, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (323, 4, 3, 'A3', 2, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (324, 5, 4, 'A4', 2, '(25,1)', 10800000, 112);
INSERT INTO sourcetrackconfig VALUES (330, 1, 1, 'V',  1, '(25,1)', 10800000, 113);
INSERT INTO sourcetrackconfig VALUES (331, 2, 1, 'A1', 2, '(25,1)', 10800000, 113);
INSERT INTO sourcetrackconfig VALUES (332, 3, 2, 'A2', 2, '(25,1)', 10800000, 113);
INSERT INTO sourcetrackconfig VALUES (333, 4, 3, 'A3', 2, '(25,1)', 10800000, 113);
INSERT INTO sourcetrackconfig VALUES (334, 5, 4, 'A4', 2, '(25,1)', 10800000, 113);

-- Simple Studio A
INSERT INTO sourcetrackconfig VALUES (400, 1, 1, 'V',  1, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (401, 2, 1, 'A1', 2, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (402, 3, 2, 'A2', 2, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (403, 4, 3, 'A3', 2, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (404, 5, 4, 'A4', 2, '(25,1)', 10800000, 120);
INSERT INTO sourcetrackconfig VALUES (410, 1, 1, 'V',  1, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (411, 2, 1, 'A1', 2, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (412, 3, 2, 'A2', 2, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (413, 4, 3, 'A3', 2, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (414, 5, 4, 'A4', 2, '(25,1)', 10800000, 121);
INSERT INTO sourcetrackconfig VALUES (420, 1, 1, 'V',  1, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (421, 2, 1, 'A1', 2, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (422, 3, 2, 'A2', 2, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (423, 4, 3, 'A3', 2, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (424, 5, 4, 'A4', 2, '(25,1)', 10800000, 122);
INSERT INTO sourcetrackconfig VALUES (430, 1, 1, 'V',  1, '(25,1)', 10800000, 123);
INSERT INTO sourcetrackconfig VALUES (431, 2, 1, 'A1', 2, '(25,1)', 10800000, 123);
INSERT INTO sourcetrackconfig VALUES (432, 3, 2, 'A2', 2, '(25,1)', 10800000, 123);
INSERT INTO sourcetrackconfig VALUES (433, 4, 3, 'A3', 2, '(25,1)', 10800000, 123);
INSERT INTO sourcetrackconfig VALUES (434, 5, 4, 'A4', 2, '(25,1)', 10800000, 123);

SELECT setval('sct_id_seq', max(sct_identifier)) FROM sourcetrackconfig;


--
-- recorder
--

INSERT INTO recorder VALUES (3, 'Ingex', NULL);
SELECT setval('rer_id_seq', max(rer_identifier)) FROM recorder;


--
-- recorderconfig
--

INSERT INTO recorderconfig VALUES (10, 'Lot', 3);
INSERT INTO recorderconfig VALUES (11, 'Stage', 3);
INSERT INTO recorderconfig VALUES (12, 'Studio A', 3);

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
    VALUES (6, 'MXF_RESOLUTION', '3', 1);
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
    VALUES (16, 'COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (17, 'TIMECODE_MODE', '1', 1);
    
--
-- recorderparameters
--

-- Simple Lot
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
    VALUES ('MXF_RESOLUTION', '3', 1, 10);
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
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 10);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 10);

-- Simple Stage
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
    VALUES ('MXF_RESOLUTION', '3', 1, 11);
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
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 11);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 11);

-- Simple Studio A
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '3', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 12);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 12);


--
-- recorder update
--

UPDATE recorder SET rer_conf_id=11 WHERE rer_identifier = 3;


--
-- recorderinputconfig
--

-- Ingex
-- Simple Lot
INSERT INTO recorderinputconfig VALUES (100, 1, 'Card 0', 10);
INSERT INTO recorderinputconfig VALUES (101, 2, 'Card 1', 10);
INSERT INTO recorderinputconfig VALUES (102, 3, 'Card 2', 10);
INSERT INTO recorderinputconfig VALUES (103, 4, 'Card 3', 10);

-- Simple Stage
INSERT INTO recorderinputconfig VALUES (110, 1, 'Card 0', 11);
INSERT INTO recorderinputconfig VALUES (111, 2, 'Card 1', 11);
INSERT INTO recorderinputconfig VALUES (112, 3, 'Card 2', 11);
INSERT INTO recorderinputconfig VALUES (113, 4, 'Card 3', 11);

-- Simple Stage
INSERT INTO recorderinputconfig VALUES (120, 1, 'Card 0', 12);
INSERT INTO recorderinputconfig VALUES (121, 2, 'Card 1', 12);
INSERT INTO recorderinputconfig VALUES (122, 3, 'Card 2', 12);
INSERT INTO recorderinputconfig VALUES (123, 4, 'Card 3', 12);


SELECT setval('ric_id_seq', max(ric_identifier)) FROM recorderinputconfig;


--
-- recorderinputtrackconfig
--

-- Simple Lot
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


-- Simple Stage
INSERT INTO recorderinputtrackconfig VALUES (500, 1, 1, 110, 110, 1);
INSERT INTO recorderinputtrackconfig VALUES (501, 2, 1, 110, 110, 2);
INSERT INTO recorderinputtrackconfig VALUES (502, 3, 2, 110, 110, 3);
INSERT INTO recorderinputtrackconfig VALUES (503, 4, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (504, 5, 0, 110, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (510, 1, 1, 111, 111, 1);
INSERT INTO recorderinputtrackconfig VALUES (511, 2, 1, 111, 111, 2);
INSERT INTO recorderinputtrackconfig VALUES (512, 3, 2, 111, 111, 3);
INSERT INTO recorderinputtrackconfig VALUES (513, 4, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (514, 5, 0, 111, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (520, 1, 1, 112, 112, 1);
INSERT INTO recorderinputtrackconfig VALUES (521, 2, 1, 112, 112, 2);
INSERT INTO recorderinputtrackconfig VALUES (522, 3, 2, 112, 112, 3);
INSERT INTO recorderinputtrackconfig VALUES (523, 4, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (524, 5, 0, 112, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (530, 1, 1, 113, 113, 1);
INSERT INTO recorderinputtrackconfig VALUES (531, 2, 1, 113, 113, 2);
INSERT INTO recorderinputtrackconfig VALUES (532, 3, 2, 113, 113, 3);
INSERT INTO recorderinputtrackconfig VALUES (533, 4, 0, 113, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (534, 5, 0, 113, NULL, NULL);

-- Simple Studio A
INSERT INTO recorderinputtrackconfig VALUES (600, 1, 1, 120, 120, 1);
INSERT INTO recorderinputtrackconfig VALUES (601, 2, 1, 120, 120, 2);
INSERT INTO recorderinputtrackconfig VALUES (602, 3, 2, 120, 120, 3);
INSERT INTO recorderinputtrackconfig VALUES (603, 4, 0, 120, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (604, 5, 0, 120, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (610, 1, 1, 121, 121, 1);
INSERT INTO recorderinputtrackconfig VALUES (611, 2, 1, 121, 121, 2);
INSERT INTO recorderinputtrackconfig VALUES (612, 3, 2, 121, 121, 3);
INSERT INTO recorderinputtrackconfig VALUES (613, 4, 0, 121, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (614, 5, 0, 121, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (620, 1, 1, 122, 122, 1);
INSERT INTO recorderinputtrackconfig VALUES (621, 2, 1, 122, 122, 2);
INSERT INTO recorderinputtrackconfig VALUES (622, 3, 2, 122, 122, 3);
INSERT INTO recorderinputtrackconfig VALUES (623, 4, 0, 122, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (624, 5, 0, 122, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (630, 1, 1, 123, 123, 1);
INSERT INTO recorderinputtrackconfig VALUES (631, 2, 1, 123, 123, 2);
INSERT INTO recorderinputtrackconfig VALUES (632, 3, 2, 123, 123, 3);
INSERT INTO recorderinputtrackconfig VALUES (633, 4, 0, 123, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (634, 5, 0, 123, NULL, NULL);



SELECT setval('rtc_id_seq', max(rtc_identifier)) FROM recorderinputtrackconfig;


--
-- multicameraclipdef
--

INSERT INTO multicameraclipdef VALUES (2, 'KW-A44 multi-camera clip');
INSERT INTO multicameraclipdef VALUES (3, 'Studio A multi-camera clip');
INSERT INTO multicameraclipdef VALUES (4, 'Lot multi-camera clip');
INSERT INTO multicameraclipdef VALUES (5, 'Stage multi-camera clip');

SELECT setval('mcd_id_seq', max(mcd_identifier)) FROM multicameraclipdef;


--
-- multicameratrackdef
--

-- KW-A44 multi-camera clip
INSERT INTO multicameratrackdef VALUES (4, 1, 1, 2);
INSERT INTO multicameratrackdef VALUES (5, 2, 1, 2);
INSERT INTO multicameratrackdef VALUES (6, 3, 2, 2);

-- Studio A (Main and Iso) multi-camera clip
INSERT INTO multicameratrackdef VALUES (10, 1, 1, 3);
INSERT INTO multicameratrackdef VALUES (11, 2, 1, 3);
INSERT INTO multicameratrackdef VALUES (12, 3, 2, 3);

-- Lot multi-camera clip
INSERT INTO multicameratrackdef VALUES (20, 1, 1, 4);
INSERT INTO multicameratrackdef VALUES (21, 2, 1, 4);
INSERT INTO multicameratrackdef VALUES (22, 3, 2, 4);

-- Stage multi-camera clip
INSERT INTO multicameratrackdef VALUES (30, 1, 1, 5);
INSERT INTO multicameratrackdef VALUES (31, 2, 1, 5);
INSERT INTO multicameratrackdef VALUES (32, 3, 2, 5);

SELECT setval('mct_id_seq', max(mct_identifier)) FROM multicameratrackdef;


--
-- multicameraselectordef
--

-- Lot
INSERT INTO multicameraselectordef VALUES (100, 1, 20, 100, 1);
INSERT INTO multicameraselectordef VALUES (101, 2, 20, 101, 1);
INSERT INTO multicameraselectordef VALUES (102, 3, 20, 102, 1);
INSERT INTO multicameraselectordef VALUES (103, 4, 20, 103, 1);
INSERT INTO multicameraselectordef VALUES (104, 1, 21, 100, 2);
INSERT INTO multicameraselectordef VALUES (105, 1, 22, 100, 3);

-- Stage
INSERT INTO multicameraselectordef VALUES (40, 1, 30, 110, 1);
INSERT INTO multicameraselectordef VALUES (41, 2, 30, 111, 1);
INSERT INTO multicameraselectordef VALUES (42, 3, 30, 112, 1);
INSERT INTO multicameraselectordef VALUES (43, 4, 30, 113, 1);
INSERT INTO multicameraselectordef VALUES (44, 1, 31, 110, 2);
INSERT INTO multicameraselectordef VALUES (45, 1, 32, 110, 3);

-- Studio A
INSERT INTO multicameraselectordef VALUES (20, 1, 10, 120, 1);
INSERT INTO multicameraselectordef VALUES (21, 2, 10, 121, 1);
INSERT INTO multicameraselectordef VALUES (22, 3, 10, 122, 1);
INSERT INTO multicameraselectordef VALUES (23, 4, 10, 123, 1);
INSERT INTO multicameraselectordef VALUES (24, 1, 11, 120, 2);
INSERT INTO multicameraselectordef VALUES (25, 1, 12, 120, 3);

SELECT setval('mcs_id_seq', max(mcs_identifier)) FROM multicameraselectordef;




--
-- Proxy definitions
---




END;

