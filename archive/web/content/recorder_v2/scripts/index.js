var enableVersionAlert = true;

// a status update is performed every 1/2 second
var statusInterval = 500;



function set_system_status(status)
{
    if (status == null)
    {
        update_tag_value("recorder-name", "", "");
        update_tag_value("recorder-version", "", "");
        update_tag_value("disk-space", "", "");
        update_tag_value("recording-time", "", "");
        update_tag_value("src-vtr-state", "", 0);
    }
    else
    {
        var versionString = status.version + " (build " + status.buildDate + ")";
        
        update_tag_value("recorder-name", status.recorderName, status.recorderName);
        update_tag_value("recorder-version", versionString, versionString);
        var diskSpaceString = get_size_string(status.diskSpace);
        update_tag_value("disk-space", diskSpaceString, diskSpaceString);
        var recordingTimeString = get_duration_string(status.recordingTime);
        recordingTimeString += " (" + status.recordingTimeIngestFormat + ")";
        update_tag_value("recording-time", recordingTimeString, recordingTimeString);
        update_tag_value("src-vtr-state", status.sourceVTRState, status.sourceVTRState);
    }
}

function status_handler()
{
    try
    {
        if (statusRequest.readyState == 4)
        {
            if (statusRequest.status != 200)
            {
                throw "request status not ok";
            }
            
            var status = eval("(" + statusRequest.responseText + ")");

            if (!check_api_version(status))
            {
                if (enableVersionAlert)
                {
                    alert("Invalid API version " + get_api_version() +
                        ". Require version " + status.apiVersion);
                    enableVersionAlert = false;
                }
                throw "Invalid API version";
            }
            enableVersionAlert = true;

            set_general_status(status, statusInterval);
            
            set_system_status(status);
            
            // set timer for next status request
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_general_status(null, statusInterval);
        set_system_status(null);
        
        // set timer for next status request
        statusTimer = setTimeout("request_status()", statusInterval);
    }
}

function request_status()
{
    statusRequest = new XMLHttpRequest();
    statusRequest.open("GET", "/recorder/status.json?system=true", true);
    statusRequest.onreadystatechange = status_handler;
    statusRequest.send(null);
}

function init_index()
{
    // get status information from the server
    request_status();
}


