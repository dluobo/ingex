--
--
-- NOTES
--
-- 1. Database version should be >= 8.1. For version < 8.1 there is a problem with 
-- checking foreign key constraints and adding "DEFERRABLE INITIALLY DEFERRED" to the end 
-- of Foreign key constraints is not 100% reliable - see message below
--
-- Philip de Nier <philip ( dot ) denier ( at ) rd ( dot ) bbc ( dot ) co ( dot ) uk> writes:
-- > The solutions I could find in the mailing lists were either to upgrade to 
-- > version 8.1 which uses "SELECT ... FOR SHARE" (I'm currently using 8.0), 
-- > stop using foreign keys or add "DEFERRABLE INITIALLY DEFERRED" to the 
-- > constraints. For now I'd prefer to use the last option.
-- 
-- > Does using "DEFERRABLE INITIALLY DEFERRED" completely solve the problem of 
-- > transactions waiting for each other to release locks resulting from "SELECT 
-- > ... FOR UPDATE"?
-- 
-- It doesn't eliminate the problem, but it does narrow the window in which
-- the lock is held by quite a bit, by postponing the FK checks until just
-- before transaction commit.
-- 
-- > How are the deferred foreign key constraints checked - are they checked one 
-- > at a time?
-- 
-- Yeah.
-- 
--			regards, tom lane




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



-- RECORDING LOCATION

CREATE SEQUENCE rlc_id_seq;
CREATE TABLE RecordingLocation
(
    rlc_identifier INTEGER DEFAULT nextval('rlc_id_seq') NOT NULL,
    rlc_name VARCHAR(256) NOT NULL,
    CONSTRAINT rlc_key UNIQUE (rlc_name),
    CONSTRAINT rlc_pkey PRIMARY KEY (rlc_identifier)
) WITHOUT OIDS;



-- SERIES

CREATE SEQUENCE srs_id_seq;
CREATE TABLE Series
(
    srs_identifier INTEGER DEFAULT nextval('srs_id_seq') NOT NULL,
    srs_name VARCHAR(256),
    CONSTRAINT srs_pkey PRIMARY KEY (srs_identifier)
) WITHOUT OIDS;



-- PROGRAMME

CREATE SEQUENCE prg_id_seq;
CREATE TABLE Programme
(
    prg_identifier INTEGER DEFAULT nextval('prg_id_seq') NOT NULL,
    prg_name VARCHAR(256),
    prg_series_id INTEGER NOT NULL,
    CONSTRAINT prg_series_fkey FOREIGN KEY (prg_series_id) REFERENCES Series (srs_identifier)
        ON DELETE CASCADE,
    CONSTRAINT prg_pkey PRIMARY KEY (prg_identifier)
) WITHOUT OIDS;



-- ITEM

CREATE SEQUENCE itm_id_seq;
CREATE TABLE Item
(
    itm_identifier INTEGER DEFAULT nextval('itm_id_seq') NOT NULL,
    itm_description VARCHAR(512),
    itm_script_section_ref VARCHAR(512)[],
    itm_order_index INTEGER NOT NULL,
    itm_programme_id INTEGER NOT NULL,
    CONSTRAINT itm_programme_fkey FOREIGN KEY (itm_programme_id) REFERENCES Programme (prg_identifier)
        ON DELETE CASCADE,
    CONSTRAINT itm_pkey PRIMARY KEY (itm_identifier)
) WITHOUT OIDS;



-- TAKE

CREATE TABLE TakeResult
(
    tkr_identifier INTEGER NOT NULL,
    tkr_name VARCHAR(256) NOT NULL,
    CONSTRAINT tkr_key UNIQUE (tkr_name),
    CONSTRAINT tkr_pkey PRIMARY KEY (tkr_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE tke_id_seq;
CREATE TABLE Take
(
    tke_identifier INTEGER DEFAULT nextval('tke_id_seq') NOT NULL,
    tke_number INTEGER NOT NULL,
    tke_comment TEXT,
    tke_result INTEGER NOT NULL,
    tke_edit_rate Rational NOT NULL,
    tke_length BIGINT NOT NULL,
    tke_start_position BIGINT NOT NULL,
    tke_start_date DATE NOT NULL,
    tke_recording_location INTEGER DEFAULT NULL,
    tke_item_id INTEGER NOT NULL,
    CONSTRAINT tke_result_fkey FOREIGN KEY (tke_result) REFERENCES TakeResult (tkr_identifier),
    CONSTRAINT tke_recording_location_fkey FOREIGN KEY (tke_recording_location) REFERENCES RecordingLocation (rlc_identifier),
    CONSTRAINT tke_item_fkey FOREIGN KEY (tke_item_id) REFERENCES Item (itm_identifier)
        ON DELETE CASCADE,
    CONSTRAINT tke_pkey PRIMARY KEY (tke_identifier)
) WITHOUT OIDS;





-- PACKAGE

CREATE TABLE EssenceDescriptorType
(
    edt_identifier INTEGER NOT NULL,
    edt_name VARCHAR(256) NOT NULL,
    CONSTRAINT edt_key UNIQUE (edt_name),
    CONSTRAINT edt_pkey PRIMARY KEY (edt_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE fft_id_seq;
CREATE TABLE FileFormat
(
    fft_identifier INTEGER DEFAULT nextval('fft_id_seq') NOT NULL,
    fft_name VARCHAR(256) NOT NULL,
    CONSTRAINT fft_key UNIQUE (fft_name),
    CONSTRAINT fft_pkey PRIMARY KEY (fft_identifier)
) WITHOUT OIDS;


CREATE TABLE VideoResolution
(
    vrn_identifier INTEGER NOT NULL,
    vrn_name VARCHAR(256) NOT NULL,
    CONSTRAINT vrn_key UNIQUE (vrn_name),
    CONSTRAINT vrn_pkey PRIMARY KEY (vrn_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE eds_id_seq;
CREATE TABLE EssenceDescriptor
(
    eds_identifier INTEGER DEFAULT nextval('eds_id_seq') NOT NULL,
    eds_essence_desc_type INTEGER NOT NULL,
    eds_file_location VARCHAR(4096) DEFAULT NULL, -- FileDescriptor
    eds_file_format INTEGER DEFAULT NULL, -- FileDescriptor
    eds_video_resolution_id INTEGER DEFAULT NULL, -- FileDescriptor - Video
    eds_image_aspect_ratio Rational DEFAULT NULL, -- FileDescriptor - Video
    eds_audio_quantization_bits INTEGER DEFAULT NULL, -- FileDescriptor - Audio
    eds_spool_number VARCHAR(256) DEFAULT NULL, -- TapeDescriptor
    eds_recording_location INTEGER DEFAULT NULL, -- RecordingDescriptor
    CONSTRAINT eds_essence_desc_type_fkey FOREIGN KEY (eds_essence_desc_type) REFERENCES EssenceDescriptorType (edt_identifier),
    CONSTRAINT eds_file_format_fkey FOREIGN KEY (eds_file_format) REFERENCES FileFormat (fft_identifier),
    CONSTRAINT eds_video_resolution_fkey FOREIGN KEY (eds_video_resolution_id) REFERENCES VideoResolution (vrn_identifier),
    CONSTRAINT eds_recording_location_fkey FOREIGN KEY (eds_recording_location) REFERENCES RecordingLocation (rlc_identifier),
    CONSTRAINT eds_pkey PRIMARY KEY (eds_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE pkg_id_seq;
CREATE TABLE Package
(
    pkg_identifier INTEGER DEFAULT nextval('pkg_id_seq') NOT NULL,
    pkg_uid CHAR(64) NOT NULL,
    pkg_name VARCHAR(256),
    pkg_creation_date TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    pkg_avid_project_name VARCHAR(256),
    pkg_descriptor_id INTEGER DEFAULT NULL, -- SourcePackage only
    CONSTRAINT pkg_uid_key UNIQUE (pkg_uid),
    CONSTRAINT pkg_descriptor_fkey FOREIGN KEY (pkg_descriptor_id) REFERENCES EssenceDescriptor (eds_identifier)
        ON DELETE SET NULL,
    CONSTRAINT pkg_pkey PRIMARY KEY (pkg_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE uct_id_seq;
CREATE TABLE UserComment
(
    uct_identifier INTEGER DEFAULT nextval('uct_id_seq') NOT NULL,
    uct_package_id INTEGER NOT NULL,
    uct_name VARCHAR(256) NOT NULL,
    uct_value VARCHAR(256) NOT NULL,
    CONSTRAINT uct_package_fkey FOREIGN KEY (uct_package_id) REFERENCES Package (pkg_identifier)
        ON DELETE CASCADE,
    CONSTRAINT uct_pkey PRIMARY KEY (uct_identifier)
) WITHOUT OIDS;




-- TRACK

CREATE TABLE DataDefinition
(
    ddf_identifier INTEGER,
    ddf_name VARCHAR(256) NOT NULL,
    CONSTRAINT ddf_key UNIQUE (ddf_name),
    CONSTRAINT ddf_pkey PRIMARY KEY (ddf_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE trk_id_seq;
CREATE TABLE Track
(
    trk_identifier INTEGER DEFAULT nextval('trk_id_seq') NOT NULL,
    trk_id INTEGER NOT NULL,
    trk_number INTEGER DEFAULT 0 NOT NULL, 
    trk_name VARCHAR(256),
    trk_data_def INTEGER NOT NULL,
    trk_edit_rate Rational NOT NULL,
    trk_package_id INTEGER NOT NULL,
    CONSTRAINT trk_package_fkey FOREIGN KEY (trk_package_id) REFERENCES Package (pkg_identifier)
        ON DELETE CASCADE,
    CONSTRAINT trk_data_def_fkey FOREIGN KEY (trk_data_def) REFERENCES DataDefinition (ddf_identifier),
    CONSTRAINT trk_pkey PRIMARY KEY (trk_identifier)
) WITHOUT OIDS;



-- SOURCE CLIP

CREATE SEQUENCE scp_id_seq;
CREATE TABLE SourceClip
(
    scp_identifier INTEGER DEFAULT nextval('scp_id_seq') NOT NULL,
    scp_track_id INTEGER NOT NULL,
    scp_source_package_uid CHAR(64) NOT NULL,
    scp_source_track_id INTEGER NOT NULL,
    scp_length BIGINT NOT NULL,
    scp_position BIGINT NOT NULL,
    CONSTRAINT scp_track_id_fkey FOREIGN KEY (scp_track_id) REFERENCES Track (trk_identifier)
        ON DELETE CASCADE,
    CONSTRAINT scp_pkey PRIMARY KEY (scp_identifier)
) WITHOUT OIDS;



-- PROXY

CREATE TABLE ProxyType
(
    pxt_identifier INTEGER NOT NULL,
    pxt_name VARCHAR(256) NOT NULL,
    CONSTRAINT pxt_key UNIQUE (pxt_name),
    CONSTRAINT pxt_pkey PRIMARY KEY (pxt_identifier)
) WITHOUT OIDS;

CREATE TABLE ProxyStatus
(
    pxs_identifier INTEGER NOT NULL,
    pxs_name VARCHAR(256) NOT NULL,
    CONSTRAINT pxs_key UNIQUE (pxs_name),
    CONSTRAINT pxs_pkey PRIMARY KEY (pxs_identifier)
) WITHOUT OIDS;

CREATE SEQUENCE pxy_id_seq;
CREATE TABLE Proxy
(
    pxy_identifier INTEGER DEFAULT nextval('pxy_id_seq') NOT NULL,
    pxy_name VARCHAR(256) NOT NULL,
    pxy_type_id INTEGER NOT NULL,
    pxy_start_date DATE NOT NULL,
    pxy_start_time BIGINT NOT NULL,
    pxy_status_id INTEGER NOT NULL,
    pxy_filename VARCHAR(4096) DEFAULT NULL,
    CONSTRAINT pxy_type_fkey FOREIGN KEY (pxy_type_id) REFERENCES ProxyType (pxt_identifier),
    CONSTRAINT pxy_status_fkey FOREIGN KEY (pxy_status_id) REFERENCES ProxyStatus (pxs_identifier),
    CONSTRAINT pxy_pkey PRIMARY KEY (pxy_identifier)
) WITHOUT OIDS;


-- PROXY TRACK

CREATE SEQUENCE pyt_id_seq;
CREATE TABLE ProxyTrack
(
    pyt_identifier INTEGER DEFAULT nextval('pyt_id_seq') NOT NULL,
    pyt_proxy_id INTEGER NOT NULL,
    pyt_track_id INTEGER NOT NULL,
    pyt_data_def_id INTEGER NOT NULL,
    CONSTRAINT pyt_proxy_fkey FOREIGN KEY (pyt_proxy_id) REFERENCES Proxy (pxy_identifier)
        ON DELETE CASCADE,
    CONSTRAINT pyt_data_def_fkey FOREIGN KEY (pyt_data_def_id) REFERENCES DataDefinition (ddf_identifier),
    CONSTRAINT pyt_pkey PRIMARY KEY (pyt_identifier)
) WITHOUT OIDS;


-- PROXY SOURCE

CREATE SEQUENCE pys_id_seq;
CREATE TABLE ProxySource
(
    pys_identifier INTEGER DEFAULT nextval('pys_id_seq') NOT NULL,
    pys_proxy_track_id INTEGER NOT NULL,
    pys_index INTEGER NOT NULL,
    pys_source_material_package_uid CHAR(64) NOT NULL,
    pys_source_material_track_id INTEGER NOT NULL,
    pys_source_file_package_uid CHAR(64) NOT NULL,
    pys_source_file_track_id INTEGER NOT NULL,
    pys_filename VARCHAR(4096) DEFAULT NULL,
    CONSTRAINT pys_proxy_track_fkey FOREIGN KEY (pys_proxy_track_id) REFERENCES ProxyTrack (pyt_identifier)
        ON DELETE CASCADE,
    CONSTRAINT pys_pkey PRIMARY KEY (pys_identifier)
) WITHOUT OIDS;





-- SOURCE CONFIG

CREATE TABLE SourceConfigType
(
    srt_identifier INTEGER NOT NULL,
    srt_name VARCHAR(256) NOT NULL,
    CONSTRAINT srt_key UNIQUE (srt_name),
    CONSTRAINT srt_pkey PRIMARY KEY (srt_identifier)
) WITHOUT OIDS;

CREATE SEQUENCE scf_id_seq;
CREATE TABLE SourceConfig
(
    scf_identifier INTEGER DEFAULT nextval('scf_id_seq') NOT NULL,
    scf_name VARCHAR(256) NOT NULL,
    scf_type INTEGER NOT NULL,
    scf_spool_number VARCHAR(256) DEFAULT NULL,
    scf_recording_location INTEGER DEFAULT NULL,
    CONSTRAINT scf_recording_location_fkey FOREIGN KEY (scf_recording_location) REFERENCES RecordingLocation (rlc_identifier),
    CONSTRAINT scf_type_fkey FOREIGN KEY (scf_type) REFERENCES SourceConfigType (srt_identifier),
    CONSTRAINT scf_name_key UNIQUE (scf_name),
    CONSTRAINT scf_pkey PRIMARY KEY (scf_identifier)
) WITHOUT OIDS;


-- SOURCE TRACK CONFIG

CREATE SEQUENCE sct_id_seq;
CREATE TABLE SourceTrackConfig
(
    sct_identifier INTEGER DEFAULT nextval('sct_id_seq') NOT NULL,
    sct_track_id INTEGER NOT NULL,
    sct_track_number INTEGER DEFAULT 0 NOT NULL, 
    sct_track_name VARCHAR(256) NOT NULL,
    sct_track_data_def INTEGER NOT NULL,
    sct_track_edit_rate Rational NOT NULL,
    sct_track_length BIGINT NOT NULL,
    sct_source_id INTEGER NOT NULL,
    CONSTRAINT sct_track_data_def_fkey FOREIGN KEY (sct_track_data_def) REFERENCES DataDefinition (ddf_identifier),
    CONSTRAINT sct_source_fkey FOREIGN KEY (sct_source_id) REFERENCES SourceConfig (scf_identifier)
        ON DELETE CASCADE,
    CONSTRAINT sct_pkey PRIMARY KEY (sct_identifier)
) WITHOUT OIDS;



-- RECORDER

CREATE SEQUENCE rer_id_seq;
CREATE TABLE Recorder
(
    rer_identifier INTEGER DEFAULT nextval('rer_id_seq') NOT NULL,
    rer_name VARCHAR(256) NOT NULL,
    rer_conf_id INTEGER DEFAULT NULL,
    CONSTRAINT rer_name_key UNIQUE (rer_name),
    CONSTRAINT rer_pkey PRIMARY KEY (rer_identifier)
) WITHOUT OIDS;


-- RECORDER CONFIG

CREATE SEQUENCE rec_id_seq;
CREATE TABLE RecorderConfig
(
    rec_identifier INTEGER DEFAULT nextval('rec_id_seq') NOT NULL,
    rec_name VARCHAR(256) NOT NULL,
    rec_recorder_id INTEGER NOT NULL,
    CONSTRAINT rec_recorder_fkey FOREIGN KEY (rec_recorder_id) REFERENCES Recorder (rer_identifier)
        ON DELETE CASCADE,
    CONSTRAINT rec_pkey PRIMARY KEY (rec_identifier)
) WITHOUT OIDS;


CREATE TABLE RecorderParameterType
(
    rpt_identifier INTEGER NOT NULL,
    rpt_name VARCHAR(256) NOT NULL,
    CONSTRAINT rpt_key UNIQUE (rpt_name),
    CONSTRAINT rpt_pkey PRIMARY KEY (rpt_identifier)
) WITHOUT OIDS;

CREATE TABLE DefaultRecorderParameter
(
    drp_identifier INTEGER NOT NULL,
    drp_name VARCHAR(256) NOT NULL,
    drp_value VARCHAR(256) NOT NULL,
    drp_type VARCHAR(256) NOT NULL,
    CONSTRAINT drp_key UNIQUE (drp_name),
    CONSTRAINT drp_pkey PRIMARY KEY (drp_identifier)
) WITHOUT OIDS;

CREATE SEQUENCE rep_id_seq;
CREATE TABLE RecorderParameter
(
    rep_identifier INTEGER DEFAULT nextval('rep_id_seq') NOT NULL,
    rep_name VARCHAR(256) NOT NULL,
    rep_value VARCHAR(256) NOT NULL,
    rep_type INTEGER NOT NULL DEFAULT 1,
    rep_recorder_conf_id INTEGER NOT NULL,
    CONSTRAINT rep_recorder_conf_fkey FOREIGN KEY (rep_recorder_conf_id) REFERENCES RecorderConfig (rec_identifier)
        ON DELETE CASCADE,
    CONSTRAINT rep_type_fkey FOREIGN KEY (rep_type) REFERENCES RecorderParameterType (rpt_identifier),
    CONSTRAINT rep_pkey PRIMARY KEY (rep_identifier)
) WITHOUT OIDS;



-- complete RECORDER

ALTER TABLE Recorder 
    ADD CONSTRAINT rer_conf_fkey FOREIGN KEY (rer_conf_id) REFERENCES RecorderConfig (rec_identifier)
        ON DELETE SET NULL;

        
-- RECORDER INPUT CONFIG

CREATE SEQUENCE ric_id_seq;
CREATE TABLE RecorderInputConfig
(
    ric_identifier INTEGER DEFAULT nextval('ric_id_seq') NOT NULL,
    ric_index INTEGER NOT NULL,
    ric_name VARCHAR(256),
    ric_recorder_conf_id INTEGER NOT NULL,
    CONSTRAINT ric_recorder_conf_fkey FOREIGN KEY (ric_recorder_conf_id) REFERENCES RecorderConfig (rec_identifier)
        ON DELETE CASCADE,
    CONSTRAINT ric_pkey PRIMARY KEY (ric_identifier)
) WITHOUT OIDS;


-- RECORDER INPUT TRACK CONFIG

CREATE SEQUENCE rtc_id_seq;
CREATE TABLE RecorderInputTrackConfig
(
    rtc_identifier INTEGER DEFAULT nextval('rtc_id_seq') NOT NULL,
    rtc_index INTEGER NOT NULL,
    rtc_track_number INTEGER DEFAULT 0 NOT NULL, 
    rtc_recorder_input_id INTEGER NOT NULL,
    rtc_source_id INTEGER,  -- NULL indicates not connected
    rtc_source_track_id INTEGER,
    CONSTRAINT rtc_source_fkey FOREIGN KEY (rtc_source_id) REFERENCES SourceConfig (scf_identifier),
    CONSTRAINT rtc_recorder_input_fkey FOREIGN KEY (rtc_recorder_input_id) REFERENCES RecorderInputConfig (ric_identifier)
        ON DELETE CASCADE,
    CONSTRAINT rtc_pkey PRIMARY KEY (rtc_identifier)
) WITHOUT OIDS;




-- MULTI-CAMERA CLIP DEFINITION

CREATE SEQUENCE mcd_id_seq;
CREATE TABLE MultiCameraClipDef
(
    mcd_identifier INTEGER DEFAULT nextval('mcd_id_seq') NOT NULL,
    mcd_name VARCHAR(256) NOT NULL,
    CONSTRAINT mcd_pkey PRIMARY KEY (mcd_identifier)
) WITHOUT OIDS;


CREATE SEQUENCE mct_id_seq;
CREATE TABLE MultiCameraTrackDef
(
    mct_identifier INTEGER DEFAULT nextval('mct_id_seq') NOT NULL,
    mct_index INTEGER NOT NULL,
    mct_track_number INTEGER DEFAULT 0 NOT NULL, 
    mct_multi_camera_def_id INTEGER NOT NULL,
    CONSTRAINT mct_multi_camera_def_fkey FOREIGN KEY (mct_multi_camera_def_id) REFERENCES MultiCameraClipDef (mcd_identifier)
        ON DELETE CASCADE,
    CONSTRAINT mct_pkey PRIMARY KEY (mct_identifier)
) WITHOUT OIDS;

CREATE SEQUENCE mcs_id_seq;
CREATE TABLE MultiCameraSelectorDef
(
    mcs_identifier INTEGER DEFAULT nextval('mcs_id_seq') NOT NULL,
    mcs_index INTEGER NOT NULL,
    mcs_multi_camera_track_def_id INTEGER NOT NULL,
    mcs_source_id INTEGER,  -- NULL indicates it is not (yet) connected
    mcs_source_track_id INTEGER,
    CONSTRAINT mcs_source_fkey FOREIGN KEY (mcs_source_id) REFERENCES SourceConfig (scf_identifier),
    CONSTRAINT mcs_multi_camera_track_def_fkey FOREIGN KEY (mcs_multi_camera_track_def_id) REFERENCES MultiCameraTrackDef (mct_identifier)
        ON DELETE CASCADE,
    CONSTRAINT mcs_pkey PRIMARY KEY (mcs_identifier)
) WITHOUT OIDS;



-- PROXY DEFINITION

CREATE SEQUENCE pxd_id_seq;
CREATE TABLE ProxyDefinition
(
    pxd_identifier INTEGER DEFAULT nextval('pxd_id_seq') NOT NULL,
    pxd_name VARCHAR(256) NOT NULL,
    pxd_type_id INTEGER NOT NULL,
    CONSTRAINT pxd_type_fkey FOREIGN KEY (pxd_type_id) REFERENCES ProxyType (pxt_identifier),
    CONSTRAINT pxd_pkey PRIMARY KEY (pxd_identifier)
) WITHOUT OIDS;


-- PROXY TRACK DEFINITION

CREATE SEQUENCE ptd_id_seq;
CREATE TABLE ProxyTrackDefinition
(
    ptd_identifier INTEGER DEFAULT nextval('ptd_id_seq') NOT NULL,
    ptd_proxy_def_id INTEGER NOT NULL,
    ptd_track_id INTEGER NOT NULL,
    ptd_data_def_id INTEGER NOT NULL,
    CONSTRAINT ptd_proxy_def_fkey FOREIGN KEY (ptd_proxy_def_id) REFERENCES ProxyDefinition (pxd_identifier)
        ON DELETE CASCADE,
    CONSTRAINT ptd_data_def_fkey FOREIGN KEY (ptd_data_def_id) REFERENCES DataDefinition (ddf_identifier),
    CONSTRAINT ptd_pkey PRIMARY KEY (ptd_identifier)
) WITHOUT OIDS;


-- PROXY SOURCE

CREATE SEQUENCE psd_id_seq;
CREATE TABLE ProxySourceDefinition
(
    psd_identifier INTEGER DEFAULT nextval('psd_id_seq') NOT NULL,
    psd_proxy_track_def_id INTEGER NOT NULL,
    psd_index INTEGER NOT NULL,
    psd_source_id INTEGER, -- NULL indicates not connected
    psd_source_track_id INTEGER,
    CONSTRAINT psd_source_fkey FOREIGN KEY (psd_source_id) REFERENCES SourceConfig (scf_identifier),
    CONSTRAINT psd_proxy_track_def_fkey FOREIGN KEY (psd_proxy_track_def_id) REFERENCES ProxyTrackDefinition (ptd_identifier)
        ON DELETE CASCADE,
    CONSTRAINT psd_pkey PRIMARY KEY (psd_identifier)
) WITHOUT OIDS;



-- MATERIAL FOR PROXY CREATION

CREATE SEQUENCE mpx_id_seq;
CREATE TABLE MaterialForProxy
(
    mpx_identifier INTEGER DEFAULT nextval('mpx_id_seq') NOT NULL,
    mpx_material_package_id INTEGER NOT NULL,
    mpx_timestamp TIMESTAMP NOT NULL DEFAULT now(),
    CONSTRAINT mpx_material_package_fkey FOREIGN KEY (mpx_material_package_id) REFERENCES Package (pkg_identifier)
        ON DELETE CASCADE,
    CONSTRAINT mpx_pkey PRIMARY KEY (mpx_identifier)
) WITHOUT OIDS;



------------------------------------
-- VIEWS
------------------------------------

CREATE VIEW ConfigInfoView AS
    SELECT 
        rer_name AS "recorder", 
        ric_index AS "input", 
        rtc_index AS "rec track", 
        rtc_track_number AS "track number", 
        scf_name AS "source", 
        sct_track_name AS "src track", 
        sct_track_edit_rate AS "edit rate", 
        sct_track_length AS "length" 
    FROM 
        Recorder,
        RecorderConfig,
        RecorderInputConfig, 
        RecorderInputTrackConfig, 
        SourceConfig, 
        SourceTrackConfig
    WHERE
        rec_recorder_id = rer_identifier AND
        ric_recorder_conf_id = rec_identifier AND 
        rtc_recorder_input_id = ric_identifier AND 
        rtc_source_id = scf_identifier AND 
        rtc_source_track_id = sct_track_id AND 
        sct_source_id = scf_identifier 
    ORDER BY
        rer_name,
        ric_identifier, 
        rtc_index;
        
        
        
CREATE VIEW MultiCamDefView AS
    SELECT 
        mcd_name AS "name", 
        mct_index AS "track", 
        mcs_index AS "selection", 
        scf_name AS "source", 
        sct_track_name AS "src track" 
    FROM 
        MultiCameraClipDef,
        MultiCameraTrackDef, 
        MultiCameraSelectorDef, 
        SourceConfig, 
        SourceTrackConfig
    WHERE 
        mcs_multi_camera_track_def_id = mct_identifier AND 
        mct_multi_camera_def_id = mcd_identifier AND 
        mcs_source_id = scf_identifier AND 
        mcs_source_track_id = sct_track_id AND 
        sct_source_id = scf_identifier 
    ORDER BY 
        mct_index,
        mcs_index;

        

CREATE VIEW ProxyDefView AS
    SELECT 
        pxd_name AS "name", 
        pxt_name AS "type", 
        ptd_track_id AS "track", 
        ddf_name AS "track type", 
        scf_name AS "src name",
        psd_source_track_id AS "src track" 
    FROM 
        ProxyDefinition
        INNER JOIN ProxyType ON (pxd_type_id = pxt_identifier)
        INNER JOIN ProxyTrackDefinition ON (ptd_proxy_def_id = pxd_identifier)
        INNER JOIN DataDefinition ON (ptd_data_def_id = ddf_identifier)
        INNER JOIN ProxySourceDefinition ON (psd_proxy_track_def_id = ptd_identifier)
        LEFT OUTER JOIN SourceConfig ON (psd_source_id = scf_identifier)
    ORDER BY 
        pxd_identifier,
        ptd_track_id,
        psd_index;

        

        

        
------------------------------------
-- RULES
------------------------------------

-- delete the essence descriptor referenced by the source package

CREATE RULE delete_essence_descriptor 
    AS ON DELETE TO Package 
    WHERE OLD.pkg_descriptor_id IS NOT NULL
    DO ALSO
    (
        DELETE FROM EssenceDescriptor WHERE eds_identifier = OLD.pkg_descriptor_id;
    );


-- record newly created material packages in the ProxyMaterial table

CREATE RULE new_material_package 
    AS ON INSERT TO Package 
    WHERE NEW.pkg_descriptor_id IS NULL
    DO ALSO
    (
        INSERT INTO MaterialForProxy (mpx_material_package_id) 
        VALUES (NEW.pkg_identifier);
    );



------------------------------------
-- DATA
------------------------------------

-- database version

INSERT INTO Version (ver_version) VALUES (8);


-- fixed enumerations

INSERT INTO TakeResult (tkr_identifier, tkr_name) VALUES (1, 'Unspecified');
INSERT INTO TakeResult (tkr_identifier, tkr_name) VALUES (2, 'Good');
INSERT INTO TakeResult (tkr_identifier, tkr_name) VALUES (3, 'NotGood');


INSERT INTO DataDefinition (ddf_identifier, ddf_name) VALUES (1, 'Picture');
INSERT INTO DataDefinition (ddf_identifier, ddf_name) VALUES (2, 'Sound');


INSERT INTO EssenceDescriptorType (edt_identifier, edt_name) VALUES (1, 'File');
INSERT INTO EssenceDescriptorType (edt_identifier, edt_name) VALUES (2, 'Tape');
INSERT INTO EssenceDescriptorType (edt_identifier, edt_name) VALUES (3, 'LiveRecording');


INSERT INTO SourceConfigType (srt_identifier, srt_name) VALUES (1, 'Tape');
INSERT INTO SourceConfigType (srt_identifier, srt_name) VALUES (2, 'LiveRecording');


INSERT INTO VideoResolution (vrn_identifier, vrn_name) VALUES (1, 'Uncompressed UYVY');
INSERT INTO VideoResolution (vrn_identifier, vrn_name) VALUES (2, 'DV-25');
INSERT INTO VideoResolution (vrn_identifier, vrn_name) VALUES (3, 'DV-50');
INSERT INTO VideoResolution (vrn_identifier, vrn_name) VALUES (4, 'MJPEG 2:1');
INSERT INTO VideoResolution (vrn_identifier, vrn_name) VALUES (5, 'MJPEG 3:1');
INSERT INTO VideoResolution (vrn_identifier, vrn_name) VALUES (6, 'MJPEG 10:1');
INSERT INTO VideoResolution (vrn_identifier, vrn_name) VALUES (7, 'MJPEG 15:1');
INSERT INTO VideoResolution (vrn_identifier, vrn_name) VALUES (8, 'MJPEG 20:1');


INSERT INTO RecorderParameterType (rpt_identifier, rpt_name) VALUES (1, 'Any');


INSERT INTO ProxyStatus (pxs_identifier, pxs_name) VALUES(1, 'NotStarted');
INSERT INTO ProxyStatus (pxs_identifier, pxs_name) VALUES(2, 'Started');
INSERT INTO ProxyStatus (pxs_identifier, pxs_name) VALUES(3, 'Completed');
INSERT INTO ProxyStatus (pxs_identifier, pxs_name) VALUES(4, 'Failed');

INSERT INTO ProxyType (pxt_identifier, pxt_name) VALUES(1, 'SimpleWeb');
INSERT INTO ProxyType (pxt_identifier, pxt_name) VALUES(2, 'QuadSplit');





-- extensible enumerations

INSERT INTO RecordingLocation (rlc_identifier, rlc_name) VALUES (1, 'Unspecified');
SELECT setval('rlc_id_seq', max(rlc_identifier)) FROM RecordingLocation;

INSERT INTO FileFormat (fft_identifier, fft_name) VALUES (1, 'Unspecified');
INSERT INTO FileFormat (fft_identifier, fft_name) VALUES (2, 'MXF');
SELECT setval('fft_id_seq', max(fft_identifier)) FROM FileFormat;


