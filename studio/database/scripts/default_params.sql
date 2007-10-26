BEGIN;


--
-- defaultrecorderparameters
--

INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (1, 'IMAGE_ASPECT', '16/9', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (2, 'TIMECODE_MODE', '2', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (3, 'COPY_COMMAND', '', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (10, 'ENCODE1_RESOLUTION', '4', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (11, 'ENCODE1_WRAPPING', '2', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (12, 'ENCODE1_DIR', '/video/mxf/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (13, 'ENCODE1_BITC', 'false', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (20, 'ENCODE2_RESOLUTION', '8', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (21, 'ENCODE2_WRAPPING', '2', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (22, 'ENCODE2_DIR', '/video/mxf/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (23, 'ENCODE2_BITC', 'false', 1);
    
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (30, 'QUAD_RESOLUTION', '21', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (31, 'QUAD_WRAPPING', '1', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (32, 'QUAD_DIR', '/video/browse/', 1);
INSERT INTO defaultrecorderparameter (drp_identifier, drp_name, drp_value, drp_type)
    VALUES (33, 'QUAD_BITC', 'true', 1);
    


END;

