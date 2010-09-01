
------------------------------------
-- TYPES
------------------------------------

CREATE TYPE Rational AS
(
    numerator INTEGER,
    denominator INTEGER
);


------------------------------------
-- TABLES
------------------------------------

-- VERSION

CREATE TABLE Version
(
    ver_identifier INTEGER DEFAULT 1 NOT NULL,
    ver_version INTEGER NOT NULL,
    CHECK (ver_identifier = 1),    
    CONSTRAINT ver_pkey PRIMARY KEY (ver_identifier)
);


-- SOURCE

CREATE TABLE SourceType
(
    srt_identifier INTEGER NOT NULL,
    srt_name VARCHAR(256) NOT NULL,
    CONSTRAINT srt_key UNIQUE (srt_name),
    CONSTRAINT srt_pkey PRIMARY KEY (srt_identifier)
) WITHOUT OIDS;

CREATE SEQUENCE src_id_seq;
CREATE TABLE Source
(
    src_identifier INTEGER DEFAULT nextval('src_id_seq') NOT NULL,
    src_type_id INTEGER NOT NULL,
    src_barcode VARCHAR(32),
    src_rec_instance INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT src_barcode_key UNIQUE (src_barcode), -- creates an index for us
    CONSTRAINT src_type_fkey FOREIGN KEY (src_type_id) REFERENCES SourceType (srt_identifier),
    CONSTRAINT src_pkey PRIMARY KEY (src_identifier)
) WITHOUT OIDS;



-- SOURCE ITEM

CREATE SEQUENCE sit_id_seq;
CREATE TABLE SourceItem
(
    sit_identifier INTEGER DEFAULT nextval('sit_id_seq') NOT NULL,
    sit_format VARCHAR(6),
    sit_prog_title VARCHAR(72),
    sit_episode_title VARCHAR(144),
    sit_tx_date DATE,
    sit_mag_prefix VARCHAR(1),
    sit_prog_no VARCHAR(8),
    sit_prod_code VARCHAR(2),
    sit_spool_status VARCHAR(1),
    sit_stock_date DATE,
    sit_spool_descr VARCHAR(29),
    sit_memo VARCHAR(120),
    sit_duration BIGINT,
    sit_spool_no VARCHAR(14),
    sit_acc_no VARCHAR(14),
    sit_cat_detail VARCHAR(10),
    sit_item_no INTEGER,
    sit_aspect_ratio_code VARCHAR(8),
    sit_source_id INTEGER NOT NULL,
    CONSTRAINT sit_source_fkey FOREIGN KEY (sit_source_id) REFERENCES Source (src_identifier)
            ON DELETE CASCADE,
    CONSTRAINT sit_pkey PRIMARY KEY (sit_identifier)
) WITHOUT OIDS;

CREATE INDEX sit_source_index ON SourceItem (sit_source_id);



------------------------------------
-- VIEWS
------------------------------------


        
------------------------------------
-- RULES
------------------------------------


------------------------------------
-- FUNCTIONS
------------------------------------


------------------------------------
-- DATA
------------------------------------

-- database version

INSERT INTO Version (ver_version) VALUES (1);


-- source types

INSERT INTO SourceType (srt_identifier, srt_name) VALUES (1, 'Video Tape');


