var statusRequest = null;
var profileListRequest = null;
var profileListLoaded = false;
var profileRequest = null;
var enableProfileListRequest = true;
var enableProfileRequest = true;
var updateProfileRequest = null;
var enableUpdateProfileRequest = true;
var enableVersionAlert = true;
var profileResult = null;

// a status update is performed every 1/2 second
var statusInterval = 500;


function set_profile_result(result)
{
    var resultE = document.getElementById("update-result");
    if (!resultE)
    {
        return;
    }
    
    if (result == null)
    {
        remove_class(resultE, "result-error");
        remove_class(resultE, "result-success");
        resultE.innerHTML = "";
        return;
    }
    
    if (result.error)
    {
        remove_class(resultE, "result-success");
        add_class(resultE, "result-error");
        resultE.innerHTML = "Update failed: " + result.errorMessage;
    }
    else
    {
        remove_class(resultE, "result-error");
        add_class(resultE, "result-success");
        resultE.innerHTML = "Update successful";
    }
    
    profileResult = resultE;
}

function update_profile_handler()
{
    try
    {
        if (updateProfileRequest.readyState == 4)
        {
            if (updateProfileRequest.status != 200)
            {
                throw "select profile not ok";
            }
            
            var result = eval("(" + updateProfileRequest.responseText + ")");

            set_profile_result(result);

            if (!result.error)
            {
                try
                {
                    profileRequest = new XMLHttpRequest();
                    profileRequest.open("GET", "/recorder/profile.json" + 
                        "?id=" + result.profileId, true);
                    profileRequest.onreadystatechange = select_profile_handler;
                    profileRequest.send(null);
                }
                catch (err)
                {
                    enableProfileRequest = true;
                    throw err;
                }
            }
            
            enableUpdateProfileRequest = true;
        }
    }
    catch (err)
    {
        var result = new Object;
        result.error = true;
        result.errorMessage = "Update request failed";
        set_profile_result(result);
        
        enableUpdateProfileRequest = true;
    }
}

function update_profile()
{
    if ((profileRequest == null || enableProfileRequest) &&
        (updateProfileRequest == null || enableUpdateProfileRequest))
    {
        enableUpdateProfileRequest = false;
        enableProfileRequest = false;
        try
        {
            var profileIdE = document.getElementById("profile-id");
            var ingestFormatSelectES = document.getElementsByName("ingest-format-select-e");
            var defaultIngestFormatSelectE = document.getElementById("default-ingest-format-select-e");
            var primaryTimecodeSelectE = document.getElementById("primary-timecode-select-e");
            
            var requestUrl = "/recorder/updateprofile.json";
            
            requestUrl += "?id=" + profileIdE.innerHTML;

            requestUrl += "&defaultingestformat=" + 
                defaultIngestFormatSelectE.options[defaultIngestFormatSelectE.selectedIndex].value;
            requestUrl += "&primarytimecode=" + 
                primaryTimecodeSelectE.options[primaryTimecodeSelectE.selectedIndex].value;

            var ingestFormatsParam = "";
            var i;
            for (i = 0; i < ingestFormatSelectES.length; i++)
            {
                if (i > 0)
                {
                    ingestFormatsParam += ",";
                }
                ingestFormatsParam += ingestFormatSelectES[i].id.substring("ingest-format-".length) + "," + 
                    ingestFormatSelectES[i].options[ingestFormatSelectES[i].selectedIndex].value;
            }
            if (ingestFormatSelectES.length > 0)
            {
                requestUrl += "&ingestformats=" + encodeURIComponent(ingestFormatsParam);
            }
            

            updateProfileRequest = new XMLHttpRequest();
            updateProfileRequest.open("GET", requestUrl, true);
            updateProfileRequest.onreadystatechange = update_profile_handler;
            updateProfileRequest.send(null);
        }
        catch (err)
        {
            enableUpdateProfileRequest = true;
            enableProfileRequest = true;
        }
    }
}

function set_profile(profile)
{
    var profileE = document.getElementById("profile");
    
    profileE.innerHTML = "";
    if (profile == null)
    {
        return;
    }
    
    var tableE = document.createElement("table");
    var rowE, colE;
    
    rowE = document.createElement("tr");
    rowE.setAttribute("style", "display: none");
    colE = document.createElement("td");
    colE.innerHTML = "Id";
    rowE.appendChild(colE);
    colE = document.createElement("td");
    colE.setAttribute("id", "profile-id");
    colE.innerHTML = profile.id;
    rowE.appendChild(colE);
    tableE.appendChild(rowE);

    rowE = document.createElement("tr");
    colE = document.createElement("td");
    colE.setAttribute("class", "property-heading");
    colE.innerHTML = "Name";
    rowE.appendChild(colE);
    colE = document.createElement("td");
    colE.innerHTML = profile.name;
    rowE.appendChild(colE);
    tableE.appendChild(rowE);

    rowE = document.createElement("tr");
    colE = document.createElement("td");
    colE.setAttribute("class", "property-heading");
    colE.innerHTML = "Ingest Formats";
    rowE.appendChild(colE);
    colE = document.createElement("td");
    var table2E = document.createElement("table");
    var i;
    for (i = 0; i < profile.ingestformats.length; i++)
    {
        var row2E, col2E;
        var select2E, option2E;

        row2E = document.createElement("tr");
        col2E = document.createElement("td");
        col2E.innerHTML = get_format_string(profile.ingestformats[i].sourceformatcode);
        row2E.appendChild(col2E);
        
        col2E = document.createElement("td");
        select2E = document.createElement("select");
        select2E.setAttribute("name", "ingest-format-select-e");
        select2E.setAttribute("id", "ingest-format-" + profile.ingestformats[i].sourceformatcode);

        option2E = document.createElement("option");
        option2E.setAttribute("selected", "true");
        option2E.setAttribute("value", profile.ingestformats[i].selected.ingestformatid);
        option2E.innerHTML = profile.ingestformats[i].selected.ingestformatname;
        select2E.appendChild(option2E);

        var j;
        for (j = 0; j < profile.ingestformats[i].alternatives.length; j++)
        {
            option2E = document.createElement("option");
            option2E.setAttribute("value", profile.ingestformats[i].alternatives[j].ingestformatid);
            add_class(option2E, "alternative-option");
            option2E.innerHTML = profile.ingestformats[i].alternatives[j].ingestformatname;
            select2E.appendChild(option2E);
        }
        
        col2E.appendChild(select2E);
        row2E.appendChild(col2E);
        
        table2E.appendChild(row2E);
    }
    colE.appendChild(table2E);
    rowE.appendChild(colE);
    tableE.appendChild(rowE);

    rowE = document.createElement("tr");
    colE = document.createElement("td");
    colE.setAttribute("class", "property-heading");
    colE.innerHTML = "Default Ingest Format";
    rowE.appendChild(colE);

    colE = document.createElement("td");
    selectE = document.createElement("select");
    selectE.setAttribute("id", "default-ingest-format-select-e");
    
    optionE = document.createElement("option");
    optionE.setAttribute("selected", "true");
    optionE.setAttribute("value", profile.defaultingestformat.selected.ingestformatid);
    optionE.innerHTML = profile.defaultingestformat.selected.ingestformatname;
    selectE.appendChild(optionE);
    
    var j;
    for (j = 0; j < profile.defaultingestformat.alternatives.length; j++)
    {
        optionE = document.createElement("option");
        optionE.setAttribute("value", profile.defaultingestformat.alternatives[j].ingestformatid);
        add_class(optionE, "alternative-option");
        optionE.innerHTML = profile.defaultingestformat.alternatives[j].ingestformatname;
        selectE.appendChild(optionE);
    }
    colE.appendChild(selectE);
    rowE.appendChild(colE);
    tableE.appendChild(rowE);

    rowE = document.createElement("tr");
    colE = document.createElement("td");
    colE.setAttribute("class", "property-heading");
    colE.innerHTML = "Primary Timecode";
    rowE.appendChild(colE);

    colE = document.createElement("td");
    selectE = document.createElement("select");
    selectE.setAttribute("id", "primary-timecode-select-e");
    
    optionE = document.createElement("option");
    optionE.setAttribute("selected", "true");
    optionE.setAttribute("value", profile.primarytimecode.selected.primarytimecodeid);
    optionE.innerHTML = profile.primarytimecode.selected.primarytimecodename;
    selectE.appendChild(optionE);
    
    var j;
    for (j = 0; j < profile.primarytimecode.alternatives.length; j++)
    {
        optionE = document.createElement("option");
        optionE.setAttribute("value", profile.primarytimecode.alternatives[j].primarytimecodeid);
        add_class(optionE, "alternative-option");
        optionE.innerHTML = profile.primarytimecode.alternatives[j].primarytimecodename;
        selectE.appendChild(optionE);
    }
    colE.appendChild(selectE);
    rowE.appendChild(colE);
    tableE.appendChild(rowE);


    profileE.appendChild(tableE);
    
    var updateButtonE = document.createElement("input");
    updateButtonE.setAttribute("id", "update-profile-button");
    updateButtonE.setAttribute("type", "button");
    updateButtonE.setAttribute("value", "Update");
    updateButtonE.setAttribute("onclick", "update_profile()");
    profileE.appendChild(updateButtonE);
    
    var updateResultE = document.createElement("div");
    updateResultE.setAttribute("id", "update-result");
    profileE.appendChild(updateResultE);
}

function select_profile_handler()
{
    try
    {
        if (profileRequest.readyState == 4)
        {
            if (profileRequest.status != 200)
            {
                throw "select profile not ok";
            }
            
            var profile = eval("(" + profileRequest.responseText + ")");

            set_profile(profile);
            set_profile_result(profileResult);
            
            enableProfileRequest = true;
        }
    }
    catch (err)
    {
        set_profile(null);
        enableProfileRequest = true;
    }
}

function select_profile(selectE)
{
    if (profileRequest == null || enableProfileRequest)
    {
        enableProfileRequest = false;
        try
        {
            profileResult = null;
            
            profileRequest = new XMLHttpRequest();
            profileRequest.open("GET", "/recorder/profile.json" + 
                "?id=" + selectE.options[selectE.selectedIndex].value, true);
            profileRequest.onreadystatechange = select_profile_handler;
            profileRequest.send(null);
        }
        catch (err)
        {
            enableProfileRequest = true;
        }
    }
}

function set_profile_list(profileList)
{
    var selectE = document.forms.selectprofileform.selectprofile;
    
    selectE.innerHTML = "";
    if (profileList == null)
    {
        profileListLoaded = false;
        return;
    }
    
    var i;
    for (i = 0; i < profileList.profiles.length; i++)
    {
        var optionE = document.createElement("option");
        if (i == 0)
        {
            optionE.setAttribute("selected", "true");
        }
        optionE.setAttribute("value", profileList.profiles[i].id);
        optionE.innerHTML = profileList.profiles[i].name;
        selectE.appendChild(optionE);
    }
    
    profileListLoaded = true;
}

function profile_list_handler()
{
    try
    {
        if (profileListRequest.readyState == 4)
        {
            if (profileListRequest.status != 200)
            {
                throw "request profile list not ok";
            }
            
            var profileList = eval("(" + profileListRequest.responseText + ")");

            set_profile_list(profileList);
            
            enableProfileListRequest = true;
        }
    }
    catch (err)
    {
        set_profile_list(null);
        enableProfileListRequest = true;
    }
}

function request_profile_list()
{
    if (profileListRequest == null || enableProfileListRequest)
    {
        enableProfileListRequest = false;
        try
        {
            profileListLoaded = false;

            profileListRequest = new XMLHttpRequest();
            profileListRequest.open("GET", "/recorder/profilelist.json", true);
            profileListRequest.onreadystatechange = profile_list_handler;
            profileListRequest.send(null);
        }
        catch (err)
        {
            enableProfileListRequest = true;
        }
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
            
            if (!profileListLoaded && enableProfileListRequest)
            {
                request_profile_list();
            }
            
            // set timer for next status request
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_general_status(null, statusInterval);

        // clear the profiles
        set_profile_list(null);
        set_profile(null);
        enableProfileListRequest = true;
        enableProfileRequest = true;

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

function init_setup()
{
    // clear profile
    set_profile(null);
    
    // get status information from the server
    request_status();
    
    // get profile list
    request_profile_list();
}


