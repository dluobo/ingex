BEGIN;



--
-- recordinglocation
--

INSERT INTO recordinglocation VALUES (2, 'Studio A');
INSERT INTO recordinglocation VALUES (3, 'Lot');
INSERT INTO recordinglocation VALUES (4, 'KW-A44');
INSERT INTO recordinglocation VALUES (5, 'Stage');
INSERT INTO recordinglocation VALUES (6, 'Studio D');
SELECT setval('rlc_id_seq', max(rlc_identifier)) FROM recordinglocation;


--
-- sourceconfig
--

INSERT INTO sourceconfig VALUES (1, 'A44-Test-1', 2, NULL, 4);
INSERT INTO sourceconfig VALUES (2, 'A44-Test-2', 2, NULL, 4);
INSERT INTO sourceconfig VALUES (3, 'A44-Test-3', 2, NULL, 4);
INSERT INTO sourceconfig VALUES (4, 'A44-Test-4', 2, NULL, 4);
INSERT INTO sourceconfig VALUES (10, 'Lot - Main', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (11, 'Lot - Iso1', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (12, 'Lot - Iso2', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (13, 'Lot - Iso3', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (14, 'Lot - Iso4', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (15, 'Lot - Audio Splits', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (16, 'Lot - Audio Mix', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (20, 'Stage - Main', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (21, 'Stage - Iso1', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (22, 'Stage - Iso2', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (23, 'Stage - Iso3', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (24, 'Stage - Iso4', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (25, 'Stage - Audio Splits', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (26, 'Stage - Audio Mix', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (30, 'Studio A - Main', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (31, 'Studio A - Iso1', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (32, 'Studio A - Iso2', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (33, 'Studio A - Iso3', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (34, 'Studio A - Iso4', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (35, 'Studio A - Audio Splits', 2, NULL, 3);
INSERT INTO sourceconfig VALUES (36, 'Studio A - Audio Mix', 2, NULL, 3);
SELECT setval('scf_id_seq', max(scf_identifier)) FROM sourceconfig;


--
-- sourcetrackconfig
--

-- KW-A44
INSERT INTO sourcetrackconfig VALUES (1, 1, 1, 'V1', 1, '(25,1)', 10800000, 1);
INSERT INTO sourcetrackconfig VALUES (2, 2, 1, 'A1', 2, '(25,1)', 10800000, 1);
INSERT INTO sourcetrackconfig VALUES (3, 3, 2, 'A2', 2, '(25,1)', 10800000, 1);
INSERT INTO sourcetrackconfig VALUES (4, 4, 3, 'A3', 2, '(25,1)', 10800000, 1);
INSERT INTO sourcetrackconfig VALUES (5, 5, 4, 'A4', 2, '(25,1)', 10800000, 1);
INSERT INTO sourcetrackconfig VALUES (11, 1, 1, 'V1', 1, '(25,1)', 10800000, 2);
INSERT INTO sourcetrackconfig VALUES (12, 2, 1, 'A1', 2, '(25,1)', 10800000, 2);
INSERT INTO sourcetrackconfig VALUES (13, 3, 2, 'A2', 2, '(25,1)', 10800000, 2);
INSERT INTO sourcetrackconfig VALUES (14, 4, 3, 'A3', 2, '(25,1)', 10800000, 2);
INSERT INTO sourcetrackconfig VALUES (15, 5, 4, 'A4', 2, '(25,1)', 10800000, 2);
INSERT INTO sourcetrackconfig VALUES (21, 1, 1, 'V1', 1, '(25,1)', 10800000, 3);
INSERT INTO sourcetrackconfig VALUES (22, 2, 1, 'A1', 2, '(25,1)', 10800000, 3);
INSERT INTO sourcetrackconfig VALUES (23, 3, 2, 'A2', 2, '(25,1)', 10800000, 3);
INSERT INTO sourcetrackconfig VALUES (24, 4, 3, 'A3', 2, '(25,1)', 10800000, 3);
INSERT INTO sourcetrackconfig VALUES (25, 5, 4, 'A4', 2, '(25,1)', 10800000, 3);
INSERT INTO sourcetrackconfig VALUES (31, 1, 1, 'V1', 1, '(25,1)', 10800000, 4);
INSERT INTO sourcetrackconfig VALUES (32, 2, 1, 'A1', 2, '(25,1)', 10800000, 4);
INSERT INTO sourcetrackconfig VALUES (33, 3, 2, 'A2', 2, '(25,1)', 10800000, 4);
INSERT INTO sourcetrackconfig VALUES (34, 4, 3, 'A3', 2, '(25,1)', 10800000, 4);
INSERT INTO sourcetrackconfig VALUES (35, 5, 4, 'A4', 2, '(25,1)', 10800000, 4);

-- Lot
INSERT INTO sourcetrackconfig VALUES (100, 1, 1, 'mixer out', 1, '(25,1)', 10800000, 10);
INSERT INTO sourcetrackconfig VALUES (101, 2, 1, 'mix left',  2, '(25,1)', 10800000, 10);
INSERT INTO sourcetrackconfig VALUES (102, 3, 2, 'mix right', 2, '(25,1)', 10800000, 10);
INSERT INTO sourcetrackconfig VALUES (103, 1, 1, 'iso1', 1, '(25,1)', 10800000, 11);
INSERT INTO sourcetrackconfig VALUES (104, 1, 1, 'iso2', 1, '(25,1)', 10800000, 12);
INSERT INTO sourcetrackconfig VALUES (105, 1, 1, 'iso3', 1, '(25,1)', 10800000, 13);
INSERT INTO sourcetrackconfig VALUES (106, 1, 1, 'iso4', 1, '(25,1)', 10800000, 14);
INSERT INTO sourcetrackconfig VALUES (107, 1, 1, 'mic1', 2, '(25,1)', 10800000, 15);
INSERT INTO sourcetrackconfig VALUES (108, 2, 2, 'mic2', 2, '(25,1)', 10800000, 15);
INSERT INTO sourcetrackconfig VALUES (109, 3, 3, 'mic3', 2, '(25,1)', 10800000, 15);
INSERT INTO sourcetrackconfig VALUES (110, 4, 4, 'mic4', 2, '(25,1)', 10800000, 15);
INSERT INTO sourcetrackconfig VALUES (111, 5, 5, 'mic5', 2, '(25,1)', 10800000, 15);
INSERT INTO sourcetrackconfig VALUES (112, 6, 6, 'mic6', 2, '(25,1)', 10800000, 15);
INSERT INTO sourcetrackconfig VALUES (113, 7, 7, 'mic7', 2, '(25,1)', 10800000, 15);
INSERT INTO sourcetrackconfig VALUES (114, 8, 8, 'mic8', 2, '(25,1)', 10800000, 15);
INSERT INTO sourcetrackconfig VALUES (115, 1, 1, 'L', 2, '(25,1)', 10800000, 16);
INSERT INTO sourcetrackconfig VALUES (116, 2, 2, 'R', 2, '(25,1)', 10800000, 16);

-- Stage
INSERT INTO sourcetrackconfig VALUES (120, 1, 1, 'mixer out', 1, '(25,1)', 10800000, 20);
INSERT INTO sourcetrackconfig VALUES (121, 2, 1, 'mix left',  2, '(25,1)', 10800000, 20);
INSERT INTO sourcetrackconfig VALUES (122, 3, 2, 'mix right', 2, '(25,1)', 10800000, 20);
INSERT INTO sourcetrackconfig VALUES (123, 1, 1, 'iso1', 1, '(25,1)', 10800000, 21);
INSERT INTO sourcetrackconfig VALUES (124, 1, 1, 'iso2', 1, '(25,1)', 10800000, 22);
INSERT INTO sourcetrackconfig VALUES (125, 1, 1, 'iso3', 1, '(25,1)', 10800000, 23);
INSERT INTO sourcetrackconfig VALUES (126, 1, 1, 'iso4', 1, '(25,1)', 10800000, 24);
INSERT INTO sourcetrackconfig VALUES (127, 1, 1, 'mic1', 2, '(25,1)', 10800000, 25);
INSERT INTO sourcetrackconfig VALUES (128, 2, 2, 'mic2', 2, '(25,1)', 10800000, 25);
INSERT INTO sourcetrackconfig VALUES (129, 3, 3, 'mic3', 2, '(25,1)', 10800000, 25);
INSERT INTO sourcetrackconfig VALUES (130, 4, 4, 'mic4', 2, '(25,1)', 10800000, 25);
INSERT INTO sourcetrackconfig VALUES (131, 5, 5, 'mic5', 2, '(25,1)', 10800000, 25);
INSERT INTO sourcetrackconfig VALUES (132, 6, 6, 'mic6', 2, '(25,1)', 10800000, 25);
INSERT INTO sourcetrackconfig VALUES (133, 7, 7, 'mic7', 2, '(25,1)', 10800000, 25);
INSERT INTO sourcetrackconfig VALUES (134, 8, 8, 'mic8', 2, '(25,1)', 10800000, 25);
INSERT INTO sourcetrackconfig VALUES (135, 1, 1, 'L', 2, '(25,1)', 10800000, 26);
INSERT INTO sourcetrackconfig VALUES (136, 2, 2, 'R', 2, '(25,1)', 10800000, 26);

-- Studio A
INSERT INTO sourcetrackconfig VALUES (140, 1, 1, 'mixer out', 1, '(25,1)', 10800000, 30);
INSERT INTO sourcetrackconfig VALUES (141, 2, 1, 'mix left',  2, '(25,1)', 10800000, 30);
INSERT INTO sourcetrackconfig VALUES (142, 3, 2, 'mix right', 2, '(25,1)', 10800000, 30);
INSERT INTO sourcetrackconfig VALUES (143, 1, 1, 'iso1', 1, '(25,1)', 10800000, 31);
INSERT INTO sourcetrackconfig VALUES (144, 1, 1, 'iso2', 1, '(25,1)', 10800000, 32);
INSERT INTO sourcetrackconfig VALUES (145, 1, 1, 'iso3', 1, '(25,1)', 10800000, 33);
INSERT INTO sourcetrackconfig VALUES (146, 1, 1, 'iso4', 1, '(25,1)', 10800000, 34);
INSERT INTO sourcetrackconfig VALUES (147, 1, 1, 'mic1', 2, '(25,1)', 10800000, 35);
INSERT INTO sourcetrackconfig VALUES (148, 2, 2, 'mic2', 2, '(25,1)', 10800000, 35);
INSERT INTO sourcetrackconfig VALUES (149, 3, 3, 'mic3', 2, '(25,1)', 10800000, 35);
INSERT INTO sourcetrackconfig VALUES (150, 4, 4, 'mic4', 2, '(25,1)', 10800000, 35);
INSERT INTO sourcetrackconfig VALUES (151, 5, 5, 'mic5', 2, '(25,1)', 10800000, 35);
INSERT INTO sourcetrackconfig VALUES (152, 6, 6, 'mic6', 2, '(25,1)', 10800000, 35);
INSERT INTO sourcetrackconfig VALUES (153, 7, 7, 'mic7', 2, '(25,1)', 10800000, 35);
INSERT INTO sourcetrackconfig VALUES (154, 8, 8, 'mic8', 2, '(25,1)', 10800000, 35);
INSERT INTO sourcetrackconfig VALUES (155, 1, 1, 'L', 2, '(25,1)', 10800000, 36);
INSERT INTO sourcetrackconfig VALUES (156, 2, 2, 'R', 2, '(25,1)', 10800000, 36);

SELECT setval('sct_id_seq', max(sct_identifier)) FROM sourcetrackconfig;


--
-- recorder
--

INSERT INTO recorder VALUES (1, 'KW-A44', NULL);
INSERT INTO recorder VALUES (3, 'Ingex', NULL);
SELECT setval('rer_id_seq', max(rer_identifier)) FROM recorder;


--
-- recorderconfig
--

INSERT INTO recorderconfig VALUES (1, 'Default', 1);
INSERT INTO recorderconfig VALUES (3, 'Studio A - Main and Iso', 3);
INSERT INTO recorderconfig VALUES (4, 'Studio A - Iso only', 3);
INSERT INTO recorderconfig VALUES (5, 'Lot - Main and Iso', 3);
INSERT INTO recorderconfig VALUES (6, 'Lot - Iso only', 3);
INSERT INTO recorderconfig VALUES (7, 'Stage - Main and Iso', 3);
INSERT INTO recorderconfig VALUES (8, 'Stage - Iso only', 3);
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

INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '3', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 1);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 1);

-- Studio A - Main and Iso
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '3', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 3);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 3);
    
-- Studio A - Iso only
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '3', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 4);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 4);
    
-- Lot - Main and Iso
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '3', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 5);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 5);
    
-- Lot - Iso only
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '3', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 6);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 6);
    
-- Stage - Main and Iso
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '3', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 7);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 7);
    
-- Stage - Iso only
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF', 'true', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW', 'false', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('QUAD_MPEG2', 'false', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO', 'true', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('IMAGE_ASPECT', '16/9', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_RESOLUTION', '3', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MPEG2_BITRATE', '5000', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_AUDIO_BITS', '32', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_AUDIO_BITS', '16', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_AUDIO_BITS', '16', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('RAW_DIR', '/video/raw/', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_DIR', '/video/mxf/', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_CREATING', 'Creating/', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('MXF_SUBDIR_FAILURES', 'Failures/', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('BROWSE_DIR', '/video/browse/', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('COPY_COMMAND', '../../processing/media_transfer/media_transfer.pl -i /bigmo1/video1/mxf_incoming -l /bigmo1/video1/mxf', 1, 8);
INSERT INTO recorderparameter (rep_name, rep_value, rep_type, rep_recorder_conf_id)
    VALUES ('TIMECODE_MODE', '1', 1, 8);
    
    

--
-- recorder update
--

UPDATE recorder SET rer_conf_id=1 WHERE rer_identifier = 1;
UPDATE recorder SET rer_conf_id=3 WHERE rer_identifier = 3;


--
-- recorderinputconfig
--

INSERT INTO recorderinputconfig VALUES (1, 1, 'Input 1', 1);
INSERT INTO recorderinputconfig VALUES (2, 2, 'Input 2', 1);
INSERT INTO recorderinputconfig VALUES (3, 3, 'Input 3', 1);
INSERT INTO recorderinputconfig VALUES (4, 4, 'Input 4', 1);

-- Ingex
-- Studio A - Main and Iso
INSERT INTO recorderinputconfig VALUES (10, 1, 'Card 0', 3);
INSERT INTO recorderinputconfig VALUES (11, 2, 'Card 1', 3);
INSERT INTO recorderinputconfig VALUES (12, 3, 'Card 2', 3);
INSERT INTO recorderinputconfig VALUES (13, 4, 'Card 3', 3);

-- Studio A - Iso only
INSERT INTO recorderinputconfig VALUES (20, 1, 'Card 0', 4);
INSERT INTO recorderinputconfig VALUES (21, 2, 'Card 1', 4);
INSERT INTO recorderinputconfig VALUES (22, 3, 'Card 2', 4);
INSERT INTO recorderinputconfig VALUES (23, 4, 'Card 3', 4);

-- Lot - Main and Iso
INSERT INTO recorderinputconfig VALUES (30, 1, 'Card 0', 5);
INSERT INTO recorderinputconfig VALUES (31, 2, 'Card 1', 5);
INSERT INTO recorderinputconfig VALUES (32, 3, 'Card 2', 5);
INSERT INTO recorderinputconfig VALUES (33, 4, 'Card 3', 5);

-- Lot - Iso only
INSERT INTO recorderinputconfig VALUES (40, 1, 'Card 0', 6);
INSERT INTO recorderinputconfig VALUES (41, 2, 'Card 1', 6);
INSERT INTO recorderinputconfig VALUES (42, 3, 'Card 2', 6);
INSERT INTO recorderinputconfig VALUES (43, 4, 'Card 3', 6);

-- Stage - Main and Iso
INSERT INTO recorderinputconfig VALUES (50, 1, 'Card 0', 7);
INSERT INTO recorderinputconfig VALUES (51, 2, 'Card 1', 7);
INSERT INTO recorderinputconfig VALUES (52, 3, 'Card 2', 7);
INSERT INTO recorderinputconfig VALUES (53, 4, 'Card 3', 7);

-- Stage - Iso only
INSERT INTO recorderinputconfig VALUES (60, 1, 'Card 0', 8);
INSERT INTO recorderinputconfig VALUES (61, 2, 'Card 1', 8);
INSERT INTO recorderinputconfig VALUES (62, 3, 'Card 2', 8);
INSERT INTO recorderinputconfig VALUES (63, 4, 'Card 3', 8);

SELECT setval('ric_id_seq', max(ric_identifier)) FROM recorderinputconfig;


--
-- recorderinputtrackconfig
--

-- KW-A44
INSERT INTO recorderinputtrackconfig VALUES (1, 1, 1, 1, 1, 1);
INSERT INTO recorderinputtrackconfig VALUES (2, 2, 1, 1, 1, 2);
INSERT INTO recorderinputtrackconfig VALUES (3, 3, 2, 1, 1, 3);
INSERT INTO recorderinputtrackconfig VALUES (4, 4, 3, 1, 1, 4);
INSERT INTO recorderinputtrackconfig VALUES (5, 5, 4, 1, 1, 5);
INSERT INTO recorderinputtrackconfig VALUES (6, 1, 1, 2, 2, 1);
INSERT INTO recorderinputtrackconfig VALUES (7, 2, 1, 2, 2, 2);
INSERT INTO recorderinputtrackconfig VALUES (8, 3, 2, 2, 2, 3);
INSERT INTO recorderinputtrackconfig VALUES (9, 4, 3, 2, 2, 4);
INSERT INTO recorderinputtrackconfig VALUES (10, 5, 4, 2, 2, 5);
INSERT INTO recorderinputtrackconfig VALUES (11, 1, 1, 3, 3, 1);
INSERT INTO recorderinputtrackconfig VALUES (12, 2, 1, 3, 3, 2);
INSERT INTO recorderinputtrackconfig VALUES (13, 3, 2, 3, 3, 3);
INSERT INTO recorderinputtrackconfig VALUES (14, 4, 3, 3, 3, 4);
INSERT INTO recorderinputtrackconfig VALUES (15, 5, 4, 3, 3, 5);
INSERT INTO recorderinputtrackconfig VALUES (16, 1, 1, 4, 4, 1);
INSERT INTO recorderinputtrackconfig VALUES (17, 2, 1, 4, 4, 2);
INSERT INTO recorderinputtrackconfig VALUES (18, 3, 2, 4, 4, 3);
INSERT INTO recorderinputtrackconfig VALUES (19, 4, 3, 4, 4, 4);
INSERT INTO recorderinputtrackconfig VALUES (20, 5, 4, 4, 4, 5);

-- Ingex
-- Studio A - Main and Iso
INSERT INTO recorderinputtrackconfig VALUES (100, 1, 1, 10, 30, 1);
INSERT INTO recorderinputtrackconfig VALUES (101, 2, 1, 10, 30, 2);
INSERT INTO recorderinputtrackconfig VALUES (102, 3, 2, 10, 30, 3);
INSERT INTO recorderinputtrackconfig VALUES (103, 4, 0, 10, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (104, 5, 0, 10, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (105, 1, 1, 11, 31, 1);
INSERT INTO recorderinputtrackconfig VALUES (106, 2, 1, 11, 35, 1);
INSERT INTO recorderinputtrackconfig VALUES (107, 3, 2, 11, 35, 2);
INSERT INTO recorderinputtrackconfig VALUES (108, 4, 3, 11, 35, 3);
INSERT INTO recorderinputtrackconfig VALUES (109, 5, 4, 11, 35, 4);
INSERT INTO recorderinputtrackconfig VALUES (110, 1, 1, 12, 32, 1);
INSERT INTO recorderinputtrackconfig VALUES (111, 2, 5, 12, 35, 5);
INSERT INTO recorderinputtrackconfig VALUES (112, 3, 6, 12, 35, 6);
INSERT INTO recorderinputtrackconfig VALUES (113, 4, 7, 12, 35, 7);
INSERT INTO recorderinputtrackconfig VALUES (114, 5, 8, 12, 35, 8);
INSERT INTO recorderinputtrackconfig VALUES (115, 1, 1, 13, 33, 1);
INSERT INTO recorderinputtrackconfig VALUES (116, 2, 0, 13, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (117, 3, 0, 13, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (118, 4, 0, 13, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (119, 5, 0, 13, NULL, NULL);

-- Studio A - Iso only
INSERT INTO recorderinputtrackconfig VALUES (120, 1, 1, 20, 31, 1);
INSERT INTO recorderinputtrackconfig VALUES (121, 2, 1, 20, 36, 1);
INSERT INTO recorderinputtrackconfig VALUES (122, 3, 2, 20, 36, 2);
INSERT INTO recorderinputtrackconfig VALUES (123, 4, 0, 20, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (124, 5, 0, 20, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (125, 1, 1, 21, 32, 1);
INSERT INTO recorderinputtrackconfig VALUES (126, 2, 1, 21, 35, 1);
INSERT INTO recorderinputtrackconfig VALUES (127, 3, 2, 21, 35, 2);
INSERT INTO recorderinputtrackconfig VALUES (128, 4, 3, 21, 35, 3);
INSERT INTO recorderinputtrackconfig VALUES (129, 5, 4, 21, 35, 4);
INSERT INTO recorderinputtrackconfig VALUES (130, 1, 1, 22, 33, 1);
INSERT INTO recorderinputtrackconfig VALUES (131, 2, 5, 22, 35, 5);
INSERT INTO recorderinputtrackconfig VALUES (132, 3, 6, 22, 35, 6);
INSERT INTO recorderinputtrackconfig VALUES (133, 4, 7, 22, 35, 7);
INSERT INTO recorderinputtrackconfig VALUES (134, 5, 8, 22, 35, 8);
INSERT INTO recorderinputtrackconfig VALUES (135, 1, 1, 23, 34, 1);
INSERT INTO recorderinputtrackconfig VALUES (136, 2, 0, 23, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (137, 3, 0, 23, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (138, 4, 0, 23, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (139, 5, 0, 23, NULL, NULL);

-- Lot - Main and Iso
INSERT INTO recorderinputtrackconfig VALUES (140, 1, 1, 30, 10, 1);
INSERT INTO recorderinputtrackconfig VALUES (141, 2, 1, 30, 10, 2);
INSERT INTO recorderinputtrackconfig VALUES (142, 3, 2, 30, 10, 3);
INSERT INTO recorderinputtrackconfig VALUES (143, 4, 0, 30, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (144, 5, 0, 30, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (145, 1, 1, 31, 11, 1);
INSERT INTO recorderinputtrackconfig VALUES (146, 2, 1, 31, 15, 1);
INSERT INTO recorderinputtrackconfig VALUES (147, 3, 2, 31, 15, 2);
INSERT INTO recorderinputtrackconfig VALUES (148, 4, 3, 31, 15, 3);
INSERT INTO recorderinputtrackconfig VALUES (149, 5, 4, 31, 15, 4);
INSERT INTO recorderinputtrackconfig VALUES (150, 1, 1, 32, 12, 1);
INSERT INTO recorderinputtrackconfig VALUES (151, 2, 5, 32, 15, 5);
INSERT INTO recorderinputtrackconfig VALUES (152, 3, 6, 32, 15, 6);
INSERT INTO recorderinputtrackconfig VALUES (153, 4, 7, 32, 15, 7);
INSERT INTO recorderinputtrackconfig VALUES (154, 5, 8, 32, 15, 8);
INSERT INTO recorderinputtrackconfig VALUES (155, 1, 1, 33, 13, 1);
INSERT INTO recorderinputtrackconfig VALUES (156, 2, 0, 33, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (157, 3, 0, 33, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (158, 4, 0, 33, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (159, 5, 0, 33, NULL, NULL);

-- Lot - Iso only
INSERT INTO recorderinputtrackconfig VALUES (160, 1, 1, 40, 11, 1);
INSERT INTO recorderinputtrackconfig VALUES (161, 2, 1, 40, 16, 1);
INSERT INTO recorderinputtrackconfig VALUES (162, 3, 2, 40, 16, 2);
INSERT INTO recorderinputtrackconfig VALUES (163, 4, 0, 40, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (164, 5, 0, 40, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (165, 1, 1, 41, 12, 1);
INSERT INTO recorderinputtrackconfig VALUES (166, 2, 1, 41, 15, 1);
INSERT INTO recorderinputtrackconfig VALUES (167, 3, 2, 41, 15, 2);
INSERT INTO recorderinputtrackconfig VALUES (168, 4, 3, 41, 15, 3);
INSERT INTO recorderinputtrackconfig VALUES (169, 5, 4, 41, 15, 4);
INSERT INTO recorderinputtrackconfig VALUES (170, 1, 1, 42, 13, 1);
INSERT INTO recorderinputtrackconfig VALUES (171, 2, 5, 42, 15, 5);
INSERT INTO recorderinputtrackconfig VALUES (172, 3, 6, 42, 15, 6);
INSERT INTO recorderinputtrackconfig VALUES (173, 4, 7, 42, 15, 7);
INSERT INTO recorderinputtrackconfig VALUES (174, 5, 8, 42, 15, 8);
INSERT INTO recorderinputtrackconfig VALUES (175, 1, 1, 43, 14, 1);
INSERT INTO recorderinputtrackconfig VALUES (176, 2, 0, 43, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (177, 3, 0, 43, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (178, 4, 0, 43, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (179, 5, 0, 43, NULL, NULL);

-- Stage - Main and Iso
INSERT INTO recorderinputtrackconfig VALUES (180, 1, 1, 50, 20, 1);
INSERT INTO recorderinputtrackconfig VALUES (181, 2, 1, 50, 20, 2);
INSERT INTO recorderinputtrackconfig VALUES (182, 3, 2, 50, 20, 3);
INSERT INTO recorderinputtrackconfig VALUES (183, 4, 0, 50, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (184, 5, 0, 50, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (185, 1, 1, 51, 21, 1);
INSERT INTO recorderinputtrackconfig VALUES (186, 2, 1, 51, 25, 1);
INSERT INTO recorderinputtrackconfig VALUES (187, 3, 2, 51, 25, 2);
INSERT INTO recorderinputtrackconfig VALUES (188, 4, 3, 51, 25, 3);
INSERT INTO recorderinputtrackconfig VALUES (189, 5, 4, 51, 25, 4);
INSERT INTO recorderinputtrackconfig VALUES (190, 1, 1, 52, 22, 1);
INSERT INTO recorderinputtrackconfig VALUES (191, 2, 5, 52, 25, 5);
INSERT INTO recorderinputtrackconfig VALUES (192, 3, 6, 52, 25, 6);
INSERT INTO recorderinputtrackconfig VALUES (193, 4, 7, 52, 25, 7);
INSERT INTO recorderinputtrackconfig VALUES (194, 5, 8, 52, 25, 8);
INSERT INTO recorderinputtrackconfig VALUES (195, 1, 1, 53, 23, 1);
INSERT INTO recorderinputtrackconfig VALUES (196, 2, 0, 53, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (197, 3, 0, 53, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (198, 4, 0, 53, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (199, 5, 0, 53, NULL, NULL);

-- Stage - Iso only
INSERT INTO recorderinputtrackconfig VALUES (200, 1, 1, 60, 21, 1);
INSERT INTO recorderinputtrackconfig VALUES (201, 2, 1, 60, 26, 1);
INSERT INTO recorderinputtrackconfig VALUES (202, 3, 2, 60, 26, 2);
INSERT INTO recorderinputtrackconfig VALUES (203, 4, 0, 60, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (204, 5, 0, 60, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (205, 1, 1, 61, 22, 1);
INSERT INTO recorderinputtrackconfig VALUES (206, 2, 1, 61, 25, 1);
INSERT INTO recorderinputtrackconfig VALUES (207, 3, 2, 61, 25, 2);
INSERT INTO recorderinputtrackconfig VALUES (208, 4, 3, 61, 25, 3);
INSERT INTO recorderinputtrackconfig VALUES (209, 5, 4, 61, 25, 4);
INSERT INTO recorderinputtrackconfig VALUES (210, 1, 1, 62, 23, 1);
INSERT INTO recorderinputtrackconfig VALUES (211, 2, 5, 62, 25, 5);
INSERT INTO recorderinputtrackconfig VALUES (212, 3, 6, 62, 25, 6);
INSERT INTO recorderinputtrackconfig VALUES (213, 4, 7, 62, 25, 7);
INSERT INTO recorderinputtrackconfig VALUES (214, 5, 8, 62, 25, 8);
INSERT INTO recorderinputtrackconfig VALUES (215, 1, 1, 63, 24, 1);
INSERT INTO recorderinputtrackconfig VALUES (216, 2, 0, 63, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (217, 3, 0, 63, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (218, 4, 0, 63, NULL, NULL);
INSERT INTO recorderinputtrackconfig VALUES (219, 5, 0, 63, NULL, NULL);


SELECT setval('rtc_id_seq', max(rtc_identifier)) FROM recorderinputtrackconfig;



--
-- Router configurations
--

-- router config
INSERT INTO RouterConfig 
    (ror_identifier, ror_name) 
VALUES 
    (1, 'KW-44 Router');
SELECT setval('ror_id_seq', max(ror_identifier)) FROM RouterConfig;


-- router input config
INSERT INTO RouterInputConfig 
    (rti_identifier, rti_index, rti_name, rti_router_conf_id, rti_source_id, rti_source_track_id) 
VALUES 
    (1, 1, 'Main', 1, 1, 1);

INSERT INTO RouterInputConfig 
    (rti_identifier, rti_index, rti_name, rti_router_conf_id, rti_source_id, rti_source_track_id) 
VALUES 
    (2, 2, 'Iso1', 1, 2, 1);

INSERT INTO RouterInputConfig 
    (rti_identifier, rti_index, rti_name, rti_router_conf_id, rti_source_id, rti_source_track_id) 
VALUES 
    (3, 3, 'Iso2', 1, 3, 1);

SELECT setval('rti_id_seq', max(rti_identifier)) FROM RouterInputConfig;


-- router output config
INSERT INTO RouterOutputConfig 
    (rto_identifier, rto_index, rto_name, rto_router_conf_id) 
VALUES 
    (1, 1, 'Out1', 1);

INSERT INTO RouterOutputConfig 
    (rto_identifier, rto_index, rto_name, rto_router_conf_id) 
VALUES 
    (2, 2, 'Out2', 1);

SELECT setval('rto_id_seq', max(rto_identifier)) FROM RouterOutputConfig;



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

-- A44
INSERT INTO multicameraselectordef VALUES (7, 1, 4, 1, 1);
INSERT INTO multicameraselectordef VALUES (8, 2, 4, 2, 1);
INSERT INTO multicameraselectordef VALUES (9, 3, 4, 3, 1);
INSERT INTO multicameraselectordef VALUES (10, 4, 4, 4, 1);
INSERT INTO multicameraselectordef VALUES (11, 1, 5, 1, 2);
INSERT INTO multicameraselectordef VALUES (12, 1, 6, 1, 3);

-- Studio A (Main and Iso)
INSERT INTO multicameraselectordef VALUES (20, 1, 10, 30, 1);
INSERT INTO multicameraselectordef VALUES (21, 2, 10, 31, 1);
INSERT INTO multicameraselectordef VALUES (22, 3, 10, 32, 1);
INSERT INTO multicameraselectordef VALUES (23, 4, 10, 33, 1);
INSERT INTO multicameraselectordef VALUES (24, 1, 11, 30, 2);
INSERT INTO multicameraselectordef VALUES (25, 1, 12, 30, 3);

-- Lot
INSERT INTO multicameraselectordef VALUES (30, 1, 20, 10, 1);
INSERT INTO multicameraselectordef VALUES (31, 2, 20, 11, 1);
INSERT INTO multicameraselectordef VALUES (32, 3, 20, 12, 1);
INSERT INTO multicameraselectordef VALUES (33, 4, 20, 13, 1);
INSERT INTO multicameraselectordef VALUES (34, 1, 21, 10, 2);
INSERT INTO multicameraselectordef VALUES (35, 1, 22, 10, 3);

-- Stage
INSERT INTO multicameraselectordef VALUES (40, 1, 30, 20, 1);
INSERT INTO multicameraselectordef VALUES (41, 2, 30, 21, 1);
INSERT INTO multicameraselectordef VALUES (42, 3, 30, 22, 1);
INSERT INTO multicameraselectordef VALUES (43, 4, 30, 23, 1);
INSERT INTO multicameraselectordef VALUES (44, 1, 31, 20, 2);
INSERT INTO multicameraselectordef VALUES (45, 1, 32, 20, 3);

SELECT setval('mcs_id_seq', max(mcs_identifier)) FROM multicameraselectordef;




--
-- Proxy definitions
---

INSERT INTO ProxyDefinition 
    (pxd_identifier, pxd_name, pxd_type_id) 
VALUES 
    (1, 'KW-44 Main', 1);
SELECT setval('pxd_id_seq', max(pxd_identifier)) FROM ProxyDefinition;


INSERT INTO ProxyTrackDefinition 
    (ptd_identifier, ptd_proxy_def_id, ptd_track_id, ptd_data_def_id)
VALUES
    (1, 1, 1, 1);
SELECT setval('ptd_id_seq', max(ptd_identifier)) FROM ProxyTrackDefinition;

INSERT INTO ProxySourceDefinition 
    (psd_identifier, psd_proxy_track_def_id, psd_index, psd_source_id, psd_source_track_id)
VALUES
    (1, 1, 1, 1, 1);
SELECT setval('psd_id_seq', max(psd_identifier)) FROM ProxySourceDefinition;


INSERT INTO ProxyTrackDefinition 
    (ptd_identifier, ptd_proxy_def_id, ptd_track_id, ptd_data_def_id)
VALUES
    (2, 1, 2, 2);
SELECT setval('ptd_id_seq', max(ptd_identifier)) FROM ProxyTrackDefinition;

INSERT INTO ProxySourceDefinition 
    (psd_identifier, psd_proxy_track_def_id, psd_index, psd_source_id, psd_source_track_id)
VALUES
    (2, 2, 1, 1, 2);
SELECT setval('psd_id_seq', max(psd_identifier)) FROM ProxySourceDefinition;

INSERT INTO ProxySourceDefinition 
    (psd_identifier, psd_proxy_track_def_id, psd_index, psd_source_id, psd_source_track_id)
VALUES
    (3, 2, 2, 1, 3);
SELECT setval('psd_id_seq', max(psd_identifier)) FROM ProxySourceDefinition;




INSERT INTO ProxyDefinition 
    (pxd_identifier, pxd_name, pxd_type_id) 
VALUES 
    (2, 'KW-44 Quad Split', 2);
SELECT setval('pxd_id_seq', max(pxd_identifier)) FROM ProxyDefinition;


INSERT INTO ProxyTrackDefinition 
    (ptd_identifier, ptd_proxy_def_id, ptd_track_id, ptd_data_def_id)
VALUES
    (3, 2, 1, 1);
SELECT setval('ptd_id_seq', max(ptd_identifier)) FROM ProxyTrackDefinition;

INSERT INTO ProxySourceDefinition 
    (psd_identifier, psd_proxy_track_def_id, psd_index, psd_source_id, psd_source_track_id)
VALUES
    (4, 3, 1, 1, 1);
SELECT setval('psd_id_seq', max(psd_identifier)) FROM ProxySourceDefinition;

INSERT INTO ProxySourceDefinition 
    (psd_identifier, psd_proxy_track_def_id, psd_index, psd_source_id, psd_source_track_id)
VALUES
    (5, 3, 2, 2, 1);
SELECT setval('psd_id_seq', max(psd_identifier)) FROM ProxySourceDefinition;

INSERT INTO ProxySourceDefinition 
    (psd_identifier, psd_proxy_track_def_id, psd_index, psd_source_id, psd_source_track_id)
VALUES
    (6, 3, 3, 3, 1);
SELECT setval('psd_id_seq', max(psd_identifier)) FROM ProxySourceDefinition;


INSERT INTO ProxyTrackDefinition 
    (ptd_identifier, ptd_proxy_def_id, ptd_track_id, ptd_data_def_id)
VALUES
    (4, 2, 2, 2);
SELECT setval('ptd_id_seq', max(ptd_identifier)) FROM ProxyTrackDefinition;

INSERT INTO ProxySourceDefinition 
    (psd_identifier, psd_proxy_track_def_id, psd_index, psd_source_id, psd_source_track_id)
VALUES
    (7, 4, 1, 1, 2);
SELECT setval('psd_id_seq', max(psd_identifier)) FROM ProxySourceDefinition;

INSERT INTO ProxySourceDefinition 
    (psd_identifier, psd_proxy_track_def_id, psd_index, psd_source_id, psd_source_track_id)
VALUES
    (8, 4, 2, 1, 3);
SELECT setval('psd_id_seq', max(psd_identifier)) FROM ProxySourceDefinition;







END;

