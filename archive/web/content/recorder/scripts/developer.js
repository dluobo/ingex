var pbCenterLeft = 2;
var pbCenterRight = 798;
var statusRequest = null;

// a status update is performed every 1/2 second
var statusInterval = 500;


function set_buffer_position(barName, barValueName, position, maxPosition)
{
    var positionBarE = document.getElementById(barName);
    var positionBarValueE = document.getElementById(barValueName);
    
    var relPos = (position <= 0) ? 0 : position / maxPosition;
    var xPos = -pbCenterRight + relPos * (pbCenterRight - pbCenterLeft);
    positionBarE.setAttribute("style", "background-position: " + xPos + "px 0px");
    
    positionBarValueE.innerHTML = position;
}

function set_buffer_positions(status)
{
    if (status.sessionStatus.state != 2) // 2 == RECORDING_SESSION_STATE
    {
        set_buffer_position("dvs-buffer-image", "dvs-buffer-value", 0, 0);
        set_buffer_position("capture-buffer-image", "capture-buffer-value", 0, 0);
        set_buffer_position("store-buffer-image", "store-buffer-value", 0, 0);
        set_buffer_position("browse-buffer-image", "browse-buffer-value", 0, 0);
        return;
    }
    
    set_buffer_position("dvs-buffer-image", "dvs-buffer-value",
        status.sessionStatus.dvsBuffersEmpty, status.sessionStatus.numDVSBuffers - 1);
    set_buffer_position("capture-buffer-image", "capture-buffer-value",
        status.sessionStatus.captureBufferPos, status.sessionStatus.ringBufferSize - 1);
    set_buffer_position("store-buffer-image", "store-buffer-value",
        status.sessionStatus.storeBufferPos, status.sessionStatus.ringBufferSize - 1);
    set_buffer_position("browse-buffer-image", "browse-buffer-value",
        status.sessionStatus.browseBufferPos, status.sessionStatus.ringBufferSize - 1);
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
            
            set_buffer_positions(status);
            
            
            // set timer for next status request
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_general_status(null, statusInterval);

        // set timer for next status request
        statusTimer = setTimeout("request_status()", statusInterval);
    }
}

function request_status()
{
    statusRequest = new XMLHttpRequest();
    statusRequest.open("GET", "/recorder/status.json?sessionrecord=true&developer=true", true);
    statusRequest.onreadystatechange = status_handler;
    statusRequest.send(null);
}


function init_developer()
{
    // get status information from the server
    request_status();
}


