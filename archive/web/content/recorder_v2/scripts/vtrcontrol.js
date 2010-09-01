var statusRequest = null;
var vtrStatusRequest = null;
var vtrControlRequest = null;
var enableVTRControlRequest = true;
var enableVersionAlert = true;


// a status update is performed every 1/2 second
var statusInterval = 500;
var vtrStatusInterval = 600;


function get_device_type_string(deviceType, deviceTypeCode)
{
    return deviceType + " (0x" + convert_to_hex(deviceTypeCode, 2) + ")";
}

function get_state_string(state)
{
    switch (state)
    {
        case 0:
            return "Not Connected";
        case 1:
            return "Remote Lockout";
        case 2:
            return "No Tape";
        case 3:
            return "Stopped";
        case 4:
            return "Paused";
        case 5:
            return "Playing";
        case 6:
            return "Fast Forwarding";
        case 7:
            return "Fast Rewinding";
        case 8:
            return "Ejecting";
        case 9:
            return "Recording";
        case 10:
            return "Seeking";
        default:
            return "Not recognized";
    }
}

function get_timecode_string(tc)
{
    return "" + 
        ((tc.hour >= 10) ? tc.hour : "0" + tc.hour) + ":" + 
        ((tc.min >= 10) ? tc.min : "0" + tc.min) + ":" + 
        ((tc.sec >= 10) ? tc.sec : "0" + tc.sec) + ":" + 
        ((tc.frame >= 10) ? tc.frame : "0" + tc.frame); 
}

function get_error_string(errorCode)
{
    if (errorCode & 0x77 == 0x00)
    {
        return "None";
    }
    
    var result = "";
    switch (errorCode & 0x07)
    {
        case 0x00: // video ok
            break;
        case 0x01:
            result = "Video Almost Good";
            break;
        case 0x02:
            result = "Video Cannot Be Determined";
            break;
        case 0x03:
            result = "Video Unclear";
            break;
        case 0x04:
            result = "Video Not Good";
            break;
    }
    
    if (result.length > 0 && errorCode & 0x70 != 0x00)
    {
        result += ", ";
    }
    switch (errorCode & 0x70)
    {
        case 0x00: // audio ok
            break;
        case 0x10:
            result += "Audio Almost Good";
            break;
        case 0x20:
            result += "Audio Cannot Be Determined";
            break;
        case 0x30:
            result += "Audio Unclear";
            break;
        case 0x40:
            result += "Audio Not Good";
            break;
    }
    
    return result;
}

function set_button_state(ele, active)
{
    if (active)
    {
        add_class(ele.parentNode, "active-vtr-button");
    }
    else
    {
        remove_class(ele.parentNode, "active-vtr-button");
    }
    set_tag_state(ele, active);
}

function set_vtr_status(status)
{
    var frButtonE = document.getElementById("fr-button");
    var playButtonE = document.getElementById("play-button");
    var stopButtonE = document.getElementById("stop-button");
    var standbyButtonE = document.getElementById("standby-button");
    var ffButtonE = document.getElementById("ff-button");
    var ejectButtonE = document.getElementById("eject-button");
    
    if (status == null)
    {
        update_tag_value("status", get_state_string(0), get_state_string(0));
        update_tag_value("device-type", get_device_type_string("Unknown", 0), 0);
        
        set_button_state(frButtonE, false);
        set_button_state(playButtonE, false);
        set_button_state(stopButtonE, false);
        set_button_state(standbyButtonE, false);
        set_button_state(ffButtonE, false);
        set_button_state(ejectButtonE, false);
        return;
    }

    update_tag_value("status", get_state_string(status.state), get_state_string(status.state));
    update_tag_value("device-type", get_device_type_string(status.deviceType, status.deviceTypeCode), status.deviceTypeCode);
    
    
    switch (status.state)
    {
        case 3: // stopped
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, true);
            set_button_state(standbyButtonE, false);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        case 4: // paused
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, true);
            set_button_state(standbyButtonE, true);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        case 5: // play
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, true);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, true);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        case 6: // fast forward
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, true);
            set_button_state(ffButtonE, true);
            set_button_state(ejectButtonE, false);
            break;
        case 7: // fast rewind
            set_button_state(frButtonE, true);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, true);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        case 8: // eject
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, false);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, true);
            break;
        case 9: // record
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, true);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        default:
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, false);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
    }
}

function vtr_status_handler()
{
    try
    {
        if (vtrStatusRequest.readyState == 4)
        {
            if (vtrStatusRequest.status != 200)
            {
                throw "request vtr status not ok";
            }
            
            var status = eval("(" + vtrStatusRequest.responseText + ")");
            
            set_vtr_status(status);
            
            // set timer for next status request
            vtrStatusTimer = setTimeout("request_vtr_status()", vtrStatusInterval)
        }
    }
    catch (err)
    {
        set_vtr_status(null);
        
        // set timer for next status request
        vtrStatusTimer = setTimeout("request_vtr_status()", vtrStatusInterval)
    }
}

function request_vtr_status()
{
    vtrStatusRequest = new XMLHttpRequest();
    vtrStatusRequest.open("GET", "/vtr/status.json", true);
    vtrStatusRequest.onreadystatechange = vtr_status_handler;
    vtrStatusRequest.send(null);
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
    statusRequest.open("GET", "/recorder/status.json", true);
    statusRequest.onreadystatechange = status_handler;
    statusRequest.send(null);
}

function vtr_control_handler()
{
    try
    {
        if (vtrControlCommandRequest.readyState == 4)
        {
            if (vtrControlCommandRequest.status != 200)
            {
                throw "vtr control request not ok";
            }
            
            enableVTRControlRequest = true;
        }
    }
    catch (err)
    {
        enableVTRControlRequest = true;
    }
}

function vtr_control_command(cmd)
{
    // set a safe keyboard focus
    set_focus("vtr-control-link");

    if (vtrControlRequest == null || enableVTRControlRequest)
    {
        enableVTRControlRequest = false;
        try
        {
            vtrControlCommandRequest = new XMLHttpRequest();
            vtrControlCommandRequest.open("GET", "/vtr/control/" + cmd, true);
            vtrControlCommandRequest.onreadystatechange = vtr_control_handler;
            vtrControlCommandRequest.send(null);
        }
        catch (err)
        {
            enableVTRControlRequest = true;
        }
    }
    
}

function toggle_standby()
{
    // set a safe keyboard focus
    set_focus("vtr-control-link");

    var standbyButtonE = document.getElementById("standby-button");
    if (standbyButtonE == null)
    {
        return;
    }

    if (get_tag_state(standbyButtonE))
    {
        vtr_control_command('standbyoff');
    }
    else
    {
        vtr_control_command('standbyon');
    }
}

function init_vtrcontrol()
{
    // set a safe keyboard focus
    set_focus("vtr-control-link");
    
    // get status information from the server
    request_status();
    request_vtr_status();
}


