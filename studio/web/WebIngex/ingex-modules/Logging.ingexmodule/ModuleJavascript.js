/*
 * Copyright (C) 2008  British Broadcasting Corporation
 * Authors: Rowan de Pomerai, Tom Cox
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

// --- Logging Module Javascript ---

// --- Options ---
// queryLoc defines the location to query for a timecode. e.g. if host name for the recorder is 192.168.1.10 and
// queryLoc = ":7000/tc" then timecode requests will be made to 192.168.1.10:7000/tc
var queryLoc = ":7000/tc";
//holds the frame rate fraction
var frameDenom;
var frameNumer;
// Frame rate
var editrate;
//whether is using dropFrame or not (true = using dropFrame)
var dropFrame;
//29.97002997
var ntscFps = (30000/1001);
//the number of recorders
var numOfRecs;
//the number of system time options
var numOfSysTimeOpts = 4;

//stores the list of series and programmes
var ILseriesData = new Object();

var tree = false;
var treeLoader = false;
var renderedExt;
var rootNode;


//constants for 29.97 drop frame
var FRAMES_PER_DF_MIN = (60*30)-2; //1798 
var FRAMES_PER_DF_TENMIN = (10 * FRAMES_PER_DF_MIN) + 2;//17982
var FRAMES_PER_DF_HOUR = (6 * FRAMES_PER_DF_TENMIN);//107892

var serverUpdatePeriod = 2000; //the number of milliseconds between each update from server.

var onLoad_Logging = function() {
	checkJSLoaded(function(){
		Ext.onReady(function(){
			Logging_init();
		});
	});
}
loaderFunctions.Logging = onLoad_Logging;

//set ext spacer image to a local file instead of a remote url
Ext.BLANK_IMAGE_URL = "../ingex/ext/resources/images/default/tree/s.gif";
var ILtc = false;
/// Initialise the interface, set up the tree, the editors etc, plus the KeyMap to enable keyboard shortcuts
function Logging_init () {

	renderedExt = false;

//check to see if tree exists/is already rendered 	
if($('takesList').innerHTML){renderedExt = true;}

	treeLoader = new Ext.tree.TreeLoader({
		dataUrl : "/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgramme.pl",
		uiProviders:{
			'take': Ext.tree.TakeNodeUI,
			'item': Ext.tree.ItemNodeUI
		}
	});
	
	treeLoader.on("beforeload", function() {
			if($('programmeSelector').selectedIndex != -1) {
	        	treeLoader.baseParams.progid = $('programmeSelector').options[$('programmeSelector').selectedIndex].value;
			} else {
				treeLoader.baseParams.progid = -1;
			}
	    });

if(!renderedExt){ //ensure that only loads tree once

		// root node	 
		rootNode = new Ext.tree.AsyncTreeNode({
			allowChildren: true,
			id: '0',	//level depth
		});

	tree = new Ext.tree.ColumnTree({
		width:'100%',
		height: 300,
		rootVisible:false,
		autoScroll:true,
		enableDD: true, /* required as of MultiSelectTree v 1.1 */
		ddAppendOnly: true,
		// title: 'Takes',
		renderTo: 'takesList',
		
		columns:[{
			header:'Name',
			width:180,
			dataIndex:'itemName'
		},{
			header:'Sequence',
			width:80,
			dataIndex:'sequence'
		},{
			header:'Take Num.',
			width:60,
			dataIndex:'takeNo'
		},{
			header:'Location',
			width:60,
			dataIndex:'location'
		},{
			header:'Date',
			width:80,
			dataIndex:'date'
		},{
			header:'In',
			width:80,
			dataIndex:'inpoint'
		},{
			header:'Out',
			width:80,
			dataIndex:'out'
		},{
			header:'Duration',
			width:80,
			dataIndex:'duration'
		},{
			header:'Result',
			width:60,
			dataIndex:'result'
		},{
			header:'Comment',
			width:220,
			dataIndex:'comment'
		}],

		loader: treeLoader,
		
		root: rootNode
	});
/*
	var treeSorter = new Ext.tree.TreeSorter(tree, {
		//folderSort:true,
		//dir: "desc",
		//leafAttr: parseInt(takeNo),
		//sortType: ILgetFramesTotal
		
	});
	*/
	// render the tree
	tree.render();

	tree.expandAll();
	
	treeEditor = new Ext.tree.ColumnTreeEditor(tree,{ 
		ignoreNoChange: true,
		editDelay: 0
	});
	
	// TODO this ?
	//tree.on("valuechange", treeSorter.updateSortParent, treeSorter);
	//these add new item/scene options to the selection dropdown on any change
	
	/*tree.on("valuechange", ILpopulateItemList);
	tree.on("append", ILpopulateItemList);
	tree.on("remove", ILpopulateItemList);
	*/
	//this ensures that takes cannot have the same number and reorders them based on start point
	/*tree.on("valuechange", ILupdateTakeNumbering);
	tree.on("append", ILupdateTakeNumbering);
	tree.on("remove", ILupdateTakeNumbering);*/
	
	//this updates the nodes within the database on any change
	//tree.on("valuechange", ILupdateNodeInDB);
}
	Ext.getDoc().addKeyMap([
		{
			key: Ext.EventObject.ENTER,
			stopEvent: true,
			ctrl: true,
			fn: ILstoreTake
		}, {
			key: Ext.EventObject.ESC,
			stopEvent: true,
			fn: ILresetTake
		}, {
			key: Ext.EventObject.UP,
			stopEvent: true,
			ctrl: true,
			fn: function(){ 
				if(tree.getSelectionModel().getSelectedNode()) {
					tree.getSelectionModel().selectPrevious(); 
				} else {
					var lastItem = tree.getRootNode().lastChild;
					if(lastItem.lastChild != null) {
						tree.getSelectionModel().select(lastItem.lastChild);
					} else {
						tree.getSelectionModel().select(lastItem);
					}
				}
			}
		}, {
			key: Ext.EventObject.DOWN,
			stopEvent: true,
			ctrl: true,
			fn: function(){ 
				if(tree.getSelectionModel().getSelectedNode()) {
					tree.getSelectionModel().selectNext(); 
				} else {
					tree.getSelectionModel().select(tree.getRootNode().firstChild);
				}
			}
		}, {
			key: Ext.EventObject.RIGHT,
			fn: ILselectNextItem,
			ctrl: true,
			stopEvent: true
		}, {
			key: Ext.EventObject.LEFT,
			fn: ILselectPrevItem,
			ctrl: true,
			stopEvent: true
		}, {
			key: Ext.EventObject.G,
			ctrl:true,
			fn: ILsetGood,
			stopEvent: true
		}, {
			key: Ext.EventObject.B,
			ctrl: true,
			fn: ILsetNoGood,
			stopEvent: true
		}, {
			key: Ext.EventObject.NUM_MULTIPLY,
			fn: ILstartStop,
			stopEvent: true
		}, {
			key: Ext.EventObject.DELETE,
			ctrl:true,
			fn: ILdeleteNode,
			stopEvent: true
		}, {
			key: Ext.EventObject.N,
			ctrl: true,
			fn: function(){ILnewNode();}, //avoid passing unexpected parameters by wrapping the call in an anonymous function
			stopEvent: true
		}, {
			key: [Ext.EventObject.P,Ext.EventObject.D],
			ctrl: true,
			fn: ILduplicateNode,
			stopEvent: true
		}, {
			key: Ext.EventObject.C,
			ctrl: true,
			shift: true,
			fn: function() {$('commentBox').focus();},
			stopEvent: true
		}
	]);

	//gets the available recorders
	ILdiscoverRecorders();
	//gets the recorder locations, series and the programmes within the series
	ILpopulateProgRecInfo();
	
	ILtc = new ingexLoggingTimecode();
	
}

/// Start/stop take timecode updates
function ILstartStop (){
	var recSel = $('recorderSelector');
	//insole.warn("takeRunning = "+ILtc.takeRunning);
	if(ILtc.takeRunning == false){
		//if no recorder is selected
		if(recSel.options[recSel.selectedIndex].value == -1){
			Ext.MessageBox.alert('ERROR starting take!', 'Unable to start take : Please select a Recorder before starting a take');
		}else{
			// START
			//insole.warn("calling start function");
			$('startStopButton').innerHTML = 'Stop';
			ILtc.startTake();
		}
	}else{
		if(recSel.options[recSel.selectedIndex].value == -1){
			Ext.MessageBox.alert('ERROR stopping take!', 'incorrect stopping of take : Please ensure a Recorder is selected before stopping a take');
			$('startStopButton').innerHTML = 'Start';
			
		}else{
			// STOP
			//insole.warn("calling stop function");
			$('startStopButton').innerHTML = 'Start';
			ILtc.stopTake();
		}
	}
}

/// Examine a node's inpoint and convert it to a number of frames
/// @param node the node to examine
/// @return the number of frames

/*function ILgetFramesTotal (node) {
	
	//insole.warn("getFRamesTotal needs fixing");
	if(typeof node.attributes.inpoint != "undefined") {
		
		if(dropFrame){
		        //insole.warn("splitting by semicolon getframestotal");
			var chunks = node.attributes.inpoint.split(";");
		}else{
			//insole.warn("splitting by colon getFramestotal")
			var chunks = node.attributes.inpoint.split(":");
		}
		
		var h = parseInt(chunks[0], 10);
		var m = parseInt(chunks[1], 10);
		var s = parseInt(chunks[2], 10);
		var f = parseInt(chunks[3], 10);
		insole.warn("from get frames total"+h+":"+m+":"+s+":"+f+"  with editrate"+editrate);
		var totframes = (Math.floor(h*60*60*editrate) + Math.floor(m*60*editrate) + Math.floor(s*editrate) + f);
		insole.warn("total frames = "+totframes)

		return (Math.floor(h*60*60*editrate) + Math.floor(m*60*editrate) + Math.floor(s*editrate) + f);
	} else {
		return -1;
	}
}
*/

/// Convert a textual timecode string (aa:bb:cc:dd) to a number of frames
/// @param text the timecode string
/// @return the number of frames
function ILgetFramesTotalFromText (text, frameDrop) {
	//insole.warn("text to convert ="+text);
	if(frameDrop){
		//insole.warn("splitting by semicolon framestotalfrom text");
		var chunks = text.split(";");
	}else{
		var chunks = text.split(":");
	}
	var h = parseInt(chunks[0], 10);
	var m = parseInt(chunks[1], 10);
	var s = parseInt(chunks[2], 10);
	var f = parseInt(chunks[3], 10);

	if(frameDrop){
		//nominal fps = 30 for 29.97
		//insole.warn("total from text"+h+":"+m+":"+s+":"+f+"  witheditrate="+editrate);
		var totFrames =  (f
			+ (s * Math.round(editrate))
                	+ ((m % 10) * FRAMES_PER_DF_MIN)
			+ ((m / 10) * FRAMES_PER_DF_TEN_MIN)
                	+ (h * FRAMES_PER_DF_HOUR));
		//insole.warn("framecount returned="+totFrames);
		return (f
			+ (s * Math.round(editrate))
                	+ ((m % 10) * FRAMES_PER_DF_MIN)
			+ ((m / 10) * FRAMES_PER_DF_TEN_MIN)
                	+ (h * FRAMES_PER_DF_HOUR));
		
	}else{
		//for 29.97 have to use base 30 for tc
		if (editrate == ntscFps)
		{
			//insole.warn("total from text"+h+":"+m+":"+s+":"+f+"  witheditrate="+editrate);
			var totFrames = (Math.floor(h*60*60* Math.round(editrate) ) + Math.floor(m*60* Math.round(editrate) ) + Math.floor(s* Math.round(editrate)) + f);
			//insole.warn("framecount returned="+totFrames);
			return (Math.floor(h*60*60* Math.round(editrate) ) + Math.floor(m*60* Math.round(editrate) ) + Math.floor(s* Math.round(editrate)) + f);
		}else{
			//insole.warn("total from text"+h+":"+m+":"+s+":"+f+"  witheditrate="+editrate);
			var totFrames = (Math.floor(h*60*60*editrate) + Math.floor(m*60*editrate) + Math.floor(s*editrate) + f);
			//insole.warn("framecount returned="+totFrames);
			return (Math.floor(h*60*60*editrate) + Math.floor(m*60*editrate) + Math.floor(s*editrate) + f);
		}
	}
	
}

/// Convert a number of frames to a timecode string
/// @param length the number of frames
/// @return the timecode string
function ILgetTextFromFramesTotal (clipLength, timEditRate) {
	var h; var m; var s; var f;
	//insole.alert("get text from frames total needs fixing");
	
	//insole.warn("framecount ="+clipLength+"edit rate="+timEditRate);
	
	s = Math.floor(clipLength/timEditRate);
	m = Math.floor(s/60);
	h = Math.floor(m/60);
	//insole.warn("frameCount="+frameCount+" minus this"+(editrate * this.s));
	f = clipLength - (timEditRate * s);
	s = s - (60 * m);
	m = m - (60 * h);
	//insole.warn("this.f ="+this.f+"  before rounding down");
	f = Math.floor(f);
			
	if (editrate == ntscFps){
		if(dropFrame){
			//insole.warn("within update from count h= "+this.h+" m="+this.m+"  s="+this.s+"  f="+this.f);
			//if using system time or just incrementing then need to work out the timecode from framecount
			var countOfFrames = clipLength;
			h = Math.floor( clipLength / FRAMES_PER_DF_HOUR);
			countOfFrames = countOfFrames % FRAMES_PER_DF_HOUR;
			var ten_min = Math.floor(countOfFrames / FRAMES_PER_DF_TENMIN);
			countOfFrames = (countOfFrames % FRAMES_PER_DF_TENMIN);
			
			// must adjust frame count to make minutes calculation work
		        // calculations from here on in just assume that within the 10 minute cycle
			// there are only DF_FRAMES_PER_MINUTE (1798) frames per minute - even for the first minute
			// in the ten minute cycle. Hence we decrement the frame count by 2 to get the minutes count
			// So for the first two frames of the ten minute cycle we get a negative frames number

			countOfFrames -= 2;

			var unit_min = Math.floor(countOfFrames / FRAMES_PER_DF_MIN);
			countOfFrames = countOfFrames % FRAMES_PER_DF_MIN;
			m = (ten_min * 10) + unit_min;
			
			// frames now contains frame in minute @ 1798 frames per minute
	        	// put the 2 frame adjustment back in to get the correct frame count
	        	// For the first two frames of the ten minute cycle, frames is negative and this
	        	// adjustment makes it non-negative. For other minutes in the cycle the frames count
	        	// goes from 0 upwards, thus this adjusment gives the required 2 frame offset
			
			countOfFrames += 2;
	
		        s = Math.floor(countOfFrames / timEditRate);
        		f = Math.floor(countOfFrames % timEditRate);
		}else{
		
		}
	}
		
	if (h < 10) { h = "0" + h; }
	if (m < 10) { m = "0" + m; }
	if (s < 10) { s = "0" + s; }
	if (f < 10) { f = "0" + f; }
	if(dropFrame){
		return h+";"+m+";"+s+";"+f;
	}else{
		return h+":"+m+":"+s+":"+f;
	}
}

/// Update the take numbering in the tree (called when a take is moved)
/*function ILupdateTakeNumbering() {
	var items = tree.getRootNode().childNodes;
	insole.warn("items are "+items);
	for (var i in items) {
		var item = items[i];
		if(typeof item != "object") continue;
		var childNodes = item.childNodes;
		insole.warn("item ="+item+"item.childnodes="+item.childNodes);
		var tn = 0;
		
		for (var take in childNodes) {
			if(typeof childNodes[take].attributes != "undefined") {
				tn++;
				if (childNodes[take].attributes.takeNo != tn){
					childNodes[take].setText(tn);
					childNodes[take].attributes.takeNo = tn;
					insole.warn("childnodes[take]="+childNodes[take]+" take="+take+"  tn="+tn+"  items[i]att.databaseID="+items[i].attributes.databseID+"  i="+i);
					ILupdateNodeInDB(childNodes[take],'takeNo',tn);
					ILupdateNodeInDB(childNodes[take],'item',items[i].attributes.databaseID);
				}
			}
		}
	}
}*/

/*function ILsequenceInfo () {
	
	
var newID = -1;
	var children = tree.getRootNode().childNodes; //get the items

	if (rootNode.lastChild != null){
		var lastItem = rootNode.lastChild;
	}

	for (var child in children) {
		if(child != "remove") {
			if (children[child].id.length > 1){
				var id = children[child].id.substring(5);
			}else {
				var id = children[child].id;
			}
			if(id > newID) newID = id;
		}
	}
	newID++;
	var tmpNode= new Ext.tree.TreeNode({
		id: newID,
		itemName: value,
		sequence: sequenceName,
		uiProvider: Ext.tree.ItemNodeUI
	});


	Ext.MessageBox.show({
		title: 'Sequence ID',
		msg: 'Please enter the Sequence',
		width: 300,
		buttons: Ext.MessageBox.OKCANCEL,
		multiline: true,
		//fn: ILsequenceInfo,
		animEl: 'takes',
		icon: Ext.MessageBox.INFO
		//import
	});	

}*/

function ILstoreItem () {
	//insole.warn("storing item ");
	var progSel = $('programmeSelector');
	var seriesSel = $('seriesSelector');

	//insole.warn("selected programme = "+progSel.value+"  SELECTED SERIES="+seriesSel.value);
	//insole.warn("selectd indxprog="+progSel.selectedIndex+"  selcted indxseries="+seriesSel.selectedIndex);
	
	//perform a check to ensure that a series and programme are selected before an attempt to add a new item is made
	if (seriesSel.value == -1){//no series selected
		Ext.MessageBox.alert('ERROR adding item!', 'Unable to add new item : Please select a series before adding a new item');
	}else{
		if(progSel.value == -1)
		{
			Ext.MessageBox.alert('ERROR adding item!', 'Unable to add new item : Please select a programme before adding a new item');
			
		}else{
			//insole.warn("now adding new item to database");
			var newID;
			//get the children of the root node (the items for this programme)
			var children = tree.getRootNode().childNodes; //get the items
			//insole.log(children.toString());
			
			//if there are items get the last one
			if (rootNode.lastChild != null){
				var lastItem = rootNode.lastChild;
				//insole.log("id="+lastItem.attributes.id+" name "+lastItem.attributes.itemName+" sequence ="+lastItem.attributes.sequence);
				newID = Number(lastItem.attributes.id) + 1;
				
			}else{//set to first item in order index
				newID = 0;
			}
				
			//insole.log("newID = "+newID);

			//TODO get item name and sequence from the text field and add to database
			var newItemName = $('itemBox').value;
			var seq = "{"+ $('seqBox').value + "}";
			
			//insole.log("adding item "+newItemName+"  seq="+seq+" progid="+progSel.value);
			
			// -- Send to database
			Ext.Ajax.request({
				url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/addItem.pl',
				params : {
					id: newID,
					name: newItemName,
					sequence: seq,
					programme: progSel.value
				},
				success: function(response) {
					try {
						var data = JSON.parse(response.responseText);
					} catch (e) {
						insole.log("JSON parse error: "+e.name+" - "+e.message);
						insole.log("JSON data was: "+response.responseText);
						insole.alert("Failed to add item to database: "+newItemName+". See previous log message.");
					}
					if(data.success) {
						//reset the item input text boxes
						ILresetItems();
						//repopulating the item list occurs automatically on any change
						//reload the tree
						ILloadNewProg();
						insole.log("Successfully added item to database: "+newItemName);
					} else {
						insole.error("Error adding to database: "+data.error);
						insole.alert("Failed to add item to database: "+newItemName+". See previous log message.");
					}
				},
				failure: function() {
					insole.alert("Failed to add item to database: "+newItemName+". Recommend you refresh this page to check data integrity.");
				}
			});
		}
			
	}
}

function ILresetItems (){
	$('itemBox').value ='';
	$('seqBox').value ='';
}


/// Create a new item
/// @param value the item name
/// @param seq the sequence reference string

 /*function ILnewNode (value, seq) {
	
	//TODO perform a check that a series and programme are selected otherwise
	//just message box saying cannot complete, please select series and programme 

	Ext.MessageBox.show({
		title: 'Item Name',
		msg: 'Please enter the item name',
		width: 300,
		buttons: Ext.MessageBox.OKCANCEL,
		multiline: true,
		fn: ILsequenceInfo,
		animEl: 'takes',
		icon: Ext.MessageBox.INFO
		//import
	});

	//if value and seq are not passed - set to default, else use passed param values
	var newItemName;
	if (isDefault(value)) {
		value = '';
		newItemName = '[new item]';
	} else {
		newItemName = value;
	}
	
	var sequenceName;
	if (isDefault(seq)) {
		seq = "{}";
		sequenceName = "[no sequence]";
	} else {
		sequenceName = seq;
		seq = "{"+seq+"}";
	}
	
	
	var newID = -1;
	var children = tree.getRootNode().childNodes; //get the items

	if (rootNode.lastChild != null){
		var lastItem = rootNode.lastChild;
	}

	for (var child in children) {
		if(child != "remove") {
			if (children[child].id.length > 1){
				var id = children[child].id.substring(5);
			}else {
				var id = children[child].id;
			}
			if(id > newID) newID = id;
		}
	}
	newID++;
	var tmpNode= new Ext.tree.TreeNode({
		id: newID,
		itemName: value,
		sequence: sequenceName,
		uiProvider: Ext.tree.ItemNodeUI
	});
	var newNode;
	if(tree) { 
		newNode = tree.getRootNode().appendChild(tmpNode);
		var el = newNode.getUI().getItemNameEl();
		treeEditor.completeEdit();
		treeEditor.editNode = newNode;
		treeEditor.targetElement = el;
		treeEditor.startEdit(el,value);
	}
	
	// -- Send to database
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/addItem.pl',
		params : {
			id: newID,
			name: newItemName,
			sequence: seq,
			programme: $('programmeSelector').options[$('programmeSelector').selectedIndex].value
		},
		success: function(response) {
			try {
				var data = JSON.parse(response.responseText);
			} catch (e) {
				insole.log("JSON parse error: "+e.name+" - "+e.message);
				insole.log("JSON data was: "+response.responseText);
				insole.alert("Failed to add item to database: "+newItemName+". See previous log message.");
			}
			if(data.success) {
				newNode.attributes.databaseID = data.id;
				insole.log("Successfully added item to database: "+newItemName);
			} else {
				insole.error("Error adding to database: "+data.error);
				insole.alert("Failed to add item to database: "+newItemName+". See previous log message.");
			}
		},
		failure: function() {
			insole.alert("Failed to add item to database: "+newItemName+". Recommend you refresh this page to check data integrity.");
		}
	});
}
*/

/// Duplicate the currently selected item
function ILduplicateNode () {
	if(tree && tree.getSelectionModel().getSelectedNode()) {
		var value = "";
		var seq = "";
		//if item is selected
		if(tree.getSelectionModel().getSelectedNode().attributes.itemName) {
			value = tree.getSelectionModel().getSelectedNode().attributes.itemName;
			seq = tree.getSelectionModel().getSelectedNode().attributes.sequence;
		}//if a take within an item is selected 
		else if (tree.getSelectionModel().getSelectedNode().parentNode && tree.getSelectionModel().getSelectedNode().parentNode.attributes.itemName){
			value = tree.getSelectionModel().getSelectedNode().parentNode.attributes.itemName;
			seq = tree.getSelectionModel().getSelectedNode().parentNode.attributes.sequence;
		}

		ILstoreItem();
	}
}

/// Delete the currently selected item
function ILdeleteNode () {
	if(tree && tree.getSelectionModel().getSelectedNode()) {
		var node = false;
		//if an item is selected
		if(tree.getSelectionModel().getSelectedNode().attributes.itemName) {
			node = tree.getSelectionModel().getSelectedNode();
			var conf = confirm('Are you sure you wish to delete the item: '+node.attributes.itemName+' and all its takes? You cannot undo this.');
		
		} // if a take is selected 
		else if (tree.getSelectionModel().getSelectedNode().parentNode.attributes.itemName){
			node = tree.getSelectionModel().getSelectedNode().parentNode;
			var conf = confirm('Are you sure you wish to delete the item: '+node.attributes.itemName+' and all its takes? You cannot undo this.');
		}
		if (conf){
			// -- Remove from database
			Ext.Ajax.request({
				url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/deleteItem.pl',
				params : {
					id: node.attributes.databaseID
				},
				success: function(response) {
					try {
						var data = JSON.parse(response.responseText);
					} catch (e) {
						insole.log("JSON parse error: "+e.name+" - "+e.message);
						insole.log("JSON data was: "+response.responseText);
						insole.alert("Failed to delete item from database: "+node.attributes.itemName+". See previous log message.");
					}
					if(data.success) {
						//reload tree view
						ILloadNewProg();
						insole.log("Successfully deleted item from database: "+node.attributes.itemName);
					} else {
						insole.error("Error adding to database: "+data.error);
						insole.alert("Failed to deleting item from database: "+node.attributes.itemName+". See previous log message.");
					}
				},
				failure: function() {
					insole.alert("Failed to delete item from database: "+node.attributes.itemName+". Recommend you refresh this page to check data integrity.");
				}
			});
			
			
		}
	}
}

/// Store the new take to database and refresh display (reload tree)
function ILstoreTake () {
	var progSel = $('programmeSelector');
	var seriesSel = $('seriesSelector');
	
	//insole.warn("selected programme = "+progSel.value+"  SELECTED SERIES="+seriesSel.value);
	//insole.warn("selectd indxprog="+progSel.selectedIndex+"  selcted indxseries="+seriesSel.selectedIndex);
	
	//perform a check to ensure that a series and programme are selected before an attempt to add a new take is made
	if (seriesSel.value == -1){//no series selected
		Ext.MessageBox.alert('ERROR adding take!', 'Unable to add new take : Please select a series before adding a new take');
	}else{
		if(progSel.value == -1)
		{
			Ext.MessageBox.alert('ERROR adding take!', 'Unable to add new take : Please select a programme before adding a new take');
			
		}else{
			var duration = $('durationBox').value;
			if(duration == "00:00:00:00"){
				Ext.MessageBox.alert('ERROR adding take!', 'Unable to add zero length duration : Please make a log before adding a take');

			}else{

				//insole.warn("adding take");
				var locSel = $("recLocSelector");
				var itemSel = $('itemSelector');
				try {
					var item = tree.getNodeById(itemSel.options[itemSel.selectedIndex].value);
					//insole.warn("item is "+item);
					//insole.warn("attributes are "+item.attributes);
					//insole.warn("node id ="+item.attributes.id+"  or "+item.attributes.databaseID);
				} catch (e) {
					insole.alert("Error getting parent Item for new take: "+e.name+" - "+e.message);
				}
				var takeNumber;
				if(item.lastChild != null) {
					takeNumber = Number(item.lastChild.attributes.takeNo) + 1;
				
				} else {
					takeNumber = 1;
				}
				
				//insole.warn("last child is"+item.lastChild+" new takeno is"+takeNumber);
				var now = new Date();
				var dd = now.getDate();
				var mm = now.getMonth() + 1; // it ranges 0-11 so add 1
				var yy = now.getFullYear();
				if (dd < 10) { dd = "0" + dd; }
				if (mm < 10) { mm = "0" + mm; }
				var sqlDate = yy+"-"+mm+"-"+dd;
				
				var locationName = locSel.options[locSel.selectedIndex].innerHTML;
				var locationID = locSel.options[locSel.selectedIndex].value;
				var result = $('resultText').innerHTML;
				var comment = $('commentBox').value;
				if (comment == "") comment  = "[no data]";
				
				var inpoint = $('inpointBox').value;
				//insole.warn("inpoint = "+inpoint);
				var outpoint = $('outpointBox').value;
				
				
				//insole.warn("dropframe status = "+dropFrame);
				if (dropFrame){var start = ILgetFramesTotalFromText(inpoint, true);
				} else {var start = ILgetFramesTotalFromText(inpoint, false);}
				var clipLength = ILgetFramesTotalFromText(duration, false);
				
				//insole.warn("inpoint= "+inpoint+" outpoint ="+outpoint+" duration = "+duration);
				//insole.warn("invalue ="+$('inpointBox').value+"  outvalue="+$('outpointBox').value+"  duration val="+$('durationBox').value);
				
				//insole.warn("sending values"+"start = "+start+"  clipLength = "+clipLength);
			
			/*	// -- Create node
				var tmpNode= new Ext.tree.TreeNode({
					takeNo: takeNumber,
					location: locationName,
					date: sqlDate,
					inpoint: inpoint,
					out: outpoint,
					duration: duration,
					result: result,
					comment: comment,
					leaf: true,
					allowChildren: false,
					uiProvider: Ext.tree.TakeNodeUI
				});
				
				// -- Add to tree
				if(tree) {
					try {
						var newNode = item.appendChild(tmpNode);
						item.expand();
						ILresetTake();
					} catch (e) {
						insole.alert("Error storing new take: "+e.name+" - "+e.message);
						insole.alert("error adding to tree");
					}
				} else {
					insole.alert("Error storing new take: tree unavailable.");
				}
				*/
				//insole.warn("numer="+frameNumer+"   denom="+frameDenom);
				var editRateCombined = "("+frameNumer+","+frameDenom+")";
				//insole.warn(editRateCombined);
				//item selector values start at 0 but the identifiers in database start at one

				var itemIdent = (Number(itemSel.value) + 1);

				// -- Send to database
				Ext.Ajax.request({
					url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/addTake.pl',
					params : {
						takeno: takeNumber,
						item: itemIdent,
						location: locationID,
						date: sqlDate,
						start: start,
						result: result,
						comment: comment,
						editrate: editRateCombined,
						clipLength: clipLength		
						
					},
					success: function(response) {
						try {
							var data = JSON.parse(response.responseText);
							
						} catch (e) {
							insole.log("JSON parse error: "+e.name+" - "+e.message);
							insole.log("JSON data was: "+response.responseText);
							insole.alert("Failed to add take to database. Item: "+item.attributes.itemName+", Take Number: "+takeNumber+". See previous log message.");
						}
						if(data.success) {
							//insole.warn("tk:"+data.takeno+":it:"+data.item+":start:"+data.start+":length:"+clipLength); 
							insole.log("Successfully added take to database. Item: "+item.attributes.itemName+", Take Number: "+takeNumber);
							//reset the values to be stored in a take to default
							ILresetTake();
							//refresh the display from the database
							ILloadNewProg();
						} else {
							insole.error("Error adding to database: "+data.error);
							insole.alert("Failed to add take to database. Item: "+item.attributes.itemName+", Take Number: "+takeNumber+". See previous log message.");
						}
					},
					failure: function() {
						insole.alert("Failed to add take to database. Item: "+item.attributes.itemName+", Take Number: "+takeNumber+". Recommend you refresh this page to check data integrity.");
					}
				});
			}
		}
	}
}

/// Reset new take details to defaults
function ILresetTake () {
	$('inpointBox').value = "00:00:00:00";
	$('outpointBox').value = "00:00:00:00";
	$('durationBox').value = "00:00:00:00";
	$('commentBox').value = "";
	$('resultText').innerHTML = "No Good";
	$('resultText').className = "noGood";
	
	
}

/// Set the new take's result to "good"
function ILsetGood() {
	$('resultText').innerHTML = "Good";
	$('resultText').className = "good";
}

/// Set the new take's result to "no good"
function ILsetNoGood() {
	$('resultText').innerHTML = "No Good";
	$('resultText').className = "noGood";
}

/// Populate/update the list of items (the tree) when a new node is added
/*
function ILpopulateItemListNew (tree,parentNode,newNode,index) {
	var selectNew = false;
	if(typeof newNode.attributes.itemName != "undefined") selectNew = true;
	ILpopulateItemList(selectNew);
}
*/

/// Populate/update the list of items (the tree) when a new node is added

/*

function ILpopulateItemList(newNode) {
	if (isDefault(newNode)) newNode = false;
	var elSel = $('itemSelector');
	var selectedID = -1;
	if(newNode) {//select the last option
		selectedID = elSel.options.length - 1;
	} else {
		selectedID = elSel.selectedIndex;
	}
	elSel.options.length=0;
	tree.getRootNode().eachChild(function(node) {
		var elOptNew = document.createElement('option');
		elOptNew.text = node.attributes.itemName;
		elOptNew.value = node.id;
		if(elSel.length == selectedID) elOptNew.selected = true;
		try {
			elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
		}
		catch(e) {
			elSel.add(elOptNew); // IE only
		}
	});
}

*/

///populates the list of items when a programme is selected
function ILpopulateItemList(){
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getItems.pl',
		params:{
			progid: $('programmeSelector').value
 		},
		success: function(response){
			try {
				var data = JSON.parse(response.responseText);
				//array to hold all the items
				var itemNames = new Object();
				itemNames = data.itemNames;
				//insole.log("DATA IS "+data.itemNames);
				var elSel = $('itemSelector');
				//TODO the JSON SENT BACK is not ordered, it needs sorting here or in the perl with a sorted request from the database or sort once get it back into here
				
				//insole.log("itemnames0="+itemNames[0].itemName);
				//insole.log("itemnames1="+itemNames[1].itemName)
				elSel.options.length=0;
				
				for (var i in itemNames)
				{
					var elOptNew = document.createElement('option');
					//insole.log("item="+i);
					elOptNew.text = itemNames[i].itemName;
					//insole.log("itemtext="+itemNames[i].itemName);
					elOptNew.value = i;
					try {
						//insole.log("adding new option");
						elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
					}
					catch(e) {
						elSel.add(elOptNew); // IE only
					}	
				}
				
			
			} catch (e) {
				insole.alert("Error loading ITEMS list");
			}
		},
		failure: function()	{
			insole.alert("Error loading items list");
		}
	});

}



/// Send changes made to the database
/// @param node the node (item or take) being changed
/// @param dataIndex the piece of information being changed
/// @param value the value it's being changed to
/*function ILupdateNodeInDB (node,dataIndex,value) {
	var url;
	var name;
	var id;
	var valueToSend = value;
	var ui;
	//insole.warn("updating node in database");
	//insole.warn("node ="+node+"  dataIndex="+dataIndex+" value = "+value);
	if(typeof node.attributes.itemName != "undefined") {
		//item
		url = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/changeItem.pl';
		name = node.attributes.itemName;
		
		id = node.attributes.databaseID;
		//insole.warn("current item name = "+name+" id= "+id);
		if(dataIndex == "itemName") {
			ui = node.getUI().getItemNameEl();
		} else if (dataIndex == "sequence") {
			ui = node.getUI().getSequenceEl();
		}
	} else {
		//take
		url = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/changeTake.pl';
		name = node.attributes.takeNo;
		
		id = node.id;
		//insole.warn("current take name = "+name+" id= "+id);
		ui = node.getUI().getTakeElement(dataIndex);
		
		if(dataIndex == "inpoint") {
			valueToSend = ILgetFramesTotalFromText(value);
			dataIndex = "start";
			// update out
			var newOut = ILgetTextFromFramesTotal(valueToSend+ILgetFramesTotalFromText(node.attributes.duration));
			node.attributes.out = newOut;
			node.getUI().getTakeElement('out').innerHTML = newOut;
		} else if (dataIndex == "out") {
			var clipLength = ILgetFramesTotalFromText(value)-ILgetFramesTotalFromText(node.attributes.inpoint);
			valueToSend = clipLength;
			dataIndex = "clipLength";
			// update duration
			var newDuration = ILgetTextFromFramesTotal(clipLength);
			node.attributes.duration = newDuration;
			node.getUI().getTakeElement('duration').innerHTML = newDuration;
		} else if (dataIndex == "duration") {
			valueToSend = ILgetFramesTotalFromText(value);
			dataIndex = "start";
			// update out
			var newOut = ILgetTextFromFramesTotal(ILgetFramesTotalFromText(node.attributes.inpoint)+valueToSend);
			node.attributes.out = newOut;
			node.getUI().getTakeElement('out').innerHTML = newOut;
		}
	}

//      if(typeof id.substring != "undefined" && (id.substring(0,5) == "take-" || id.substring(0,5) == "item=")) id = id.substring(5);

	// -- Send to database
	Ext.Ajax.request({
		url: url,
		params : {
			dataindex: dataIndex,
			value: valueToSend,
			id: id
		},
		success: function(response) {
			try {
				var data = JSON.parse(response.responseText);
			} catch (e) {
				insole.log("JSON parse error: "+e.name+" - "+e.message);
				insole.log("JSON data was: "+response.responseText);
				insole.alert("Failed to update database for: "+name+". See previous log message.");
				Ext.MessageBox.confirm('Database Write Error', "Failed to delete item from database: "+node.attributes.itemName+". Would you like to refresh the data to see the current database state?.", function(response){
					if(response == "yes") {
						ILloadNewProg();
					}
				});
			}
			if(data.success) {
				if (typeof ui != "undefined" && typeof ui.innerHTML != "undefined") ui.innerHTML = data.value;
				if (typeof node.attributes[dataIndex] != "undefined") node.attributes[dataIndex] = data.value;
				insole.log("Successfully updated database for: "+name);
			} else {
				Ext.MessageBox.confirm('Database Write Error', "Failed to delete item from database: "+node.attributes.itemName+". Would you like to refresh the data to see the current database state?.", function(response){
					if(response == "yes") {
						ILloadNewProg();
					}
				});
				insole.error("Error updating database: "+data.error);
				insole.alert("Failed to update database for: "+name+". See previous log message.");
			}
		},
		failure: function() {
			Ext.MessageBox.confirm('Database Write Error', "Failed to delete item from database: "+node.attributes.itemName+". Would you like to refresh the data to see the current database state?.", function(response){
				if(response == "yes") {
					ILloadNewProg();
				}
			});
			insole.alert("Failed to update database for: "+name+". Recommend you refresh this page to check data integrity.");
		}
	});
}

*/

/// Select the previous item in the item list
function ILselectNextItem() {
	var elSel = $('itemSelector');
	if(typeof elSel.options[elSel.selectedIndex+1] != "undefined") elSel.options[elSel.selectedIndex+1].selected = true;
}

/// Select the next item in the item list
function ILselectPrevItem() {
	var elSel = $('itemSelector');
	if(typeof elSel.options[elSel.selectedIndex-1] != "undefined") elSel.options[elSel.selectedIndex-1].selected = true;
}

/// Populate a list of available recorders from database information
var ILRecorders = new Object();
function ILdiscoverRecorders() {
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/discoverRecorders.pl',
		success: function(response){
			try {
				var nodes = JSON.parse(response.responseText);
			} catch (e) {
				insole.alert("Error loading recorder list");
			}
			var elSel = $('recorderSelector');
			elSel.options.length=0;
			var elOptNew = document.createElement('option');
			elOptNew.text = "Choose Recorder";
			elOptNew.value = "-1";
			elOptNew.selected = true;
			try {
				elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
			} catch (e) {
				elSel.add(elOptNew); // IE only
			}
			
			numOfRecs = 0;
			for (node in nodes) {
				if(nodes[node].nodeType != "Recorder") continue;
				var elOptNew = document.createElement('option');
				elOptNew.text = node;
				elOptNew.value = numOfRecs;
				numOfRecs++;
				try {
					elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
				} catch (e) {
					elSel.add(elOptNew); // IE only
				}
			}
			ILRecorders = nodes;
			
			for (var i=0; i<numOfSysTimeOpts; i++)
			{			
				var elOptNew = document.createElement('option');
				if(i==0){elOptNew.text = "25fps Local Time";}
				if(i==1){elOptNew.text = "29.97fps non-drop Local Time";}
				if(i==2){elOptNew.text = "29.97fps drop Local Time";}
				if(i==3){elOptNew.text = "30fps non-drop Local Time";}
				elOptNew.value = (numOfRecs + i);
				try{
					//insole.warn("editrate set to ="+editrate);
					elSel.add(elOptNew, null); //standards compliant; doesn't work in IE
				}catch (e){
					elSel.add(elOptNew); //IE only
				}
			}			
			
		},
		failure: function()	{
			insole.alert("Error loading recorder list");
		}
	});
}

/// Construct the URL to query a recorder
/// @param recName the recorder's name
/// @return the url
/// Do not query if no recorder exists or no recorder selected
function ILgetRecorderURL (recName) {
	if(typeof( ILRecorders[recName] != "undefined")) {
		return ILRecorders[recName].ip+queryLoc;
	}
	return false;
}


/// Grab the series and programme lists from the database
function ILpopulateProgRecInfo() {
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/getProgRecInfo.pl',
		success: function(response){
			try {
				var data = JSON.parse(response.responseText);
			} catch (e) {
				insole.alert("Error loading series, programme & recording location lists");
			}
			
			var elSel = $('recLocSelector');
			elSel.options.length=0;
			for (loc in data.recLocs) {
				if(typeof data.recLocs[loc] != "string") continue;
				var elOptNew = document.createElement('option');
				elOptNew.text = data.recLocs[loc];
				elOptNew.value = loc;
				try {
					elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
				} catch (e) {
					elSel.add(elOptNew); // IE only
				}
			}
			
			elSel = $('seriesSelector');
			elSel.options.length=0;
			var elOptNew = document.createElement('option');
			elOptNew.text = "Choose Series";
			elOptNew.value = "-1";
			elOptNew.selected = true;
			try {
				elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
			} catch (e) {
				elSel.add(elOptNew); // IE only
			}
			for (var series in data.series) {
				if(typeof data.series[series] == "function") continue;
				var elOptNew = document.createElement('option');
				elOptNew.text = data.series[series].name;
				elOptNew.value = series;
				try {
					elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
				} catch (e) {
					elSel.add(elOptNew); // IE only
				}
			}
			
			ILseriesData = data.series;
			//make programme and item selectors unavailable until a series is selected
			$('programmeSelector').disabled = true;
			$('itemSelector').disabled = true;
		},
		failure: function()	{
			insole.alert("Error loading series, programme & recording location lists");
		}
	});
}

/// Populate the list of programmes based on the selected series
function ILupdateProgrammes () {
	var seriesID = $('seriesSelector').options[$('seriesSelector').selectedIndex].value;
	//as the series has changed reset the programmes and reload the tree
	$('programmeSelector').selectedIndex = -1;
	ILloadNewProg();
	//reset the programme
	var elSel = $('programmeSelector');
	elSel.options.length=0;
	var elOptNew = document.createElement('option');
	elOptNew.text = "Choose:";
	elOptNew.value = "-1";
	elOptNew.selected = true;
	try {
		elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
	} catch (e) {
		elSel.add(elOptNew); // IE only
	}
	if (seriesID == -1){
		//disable the programme item selectors and set them to show nothing
		$('programmeSelector').disabled=true;
		$('itemSelector').disabled=true;
		$('programmeSelector').selectedIndex = 0;
		$('itemSelector').selectedIndex = 0;
		
		
	}else{//a series is selected
		//enable the programme selector
		$('programmeSelector').disabled=false;
		var programmes = ILseriesData[seriesID].programmes;

		for (var prog in programmes) {
			if(typeof programmes[prog] != "string") continue;
			var elOptNew = document.createElement('option');
			elOptNew.text = programmes[prog];
			elOptNew.value = prog;
			try {
				elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
			} catch (e) {
				elSel.add(elOptNew); // IE only
			}
		}
	}
}

/// Grab a the currently selected programme's item/take info from the database 
function ILloadNewProg () {
	
	if($('programmeSelector').selectedIndex == -1 ||$('programmeSelector').selectedIndex == 0){
		//if no programme selected disable the item selection
		$('itemSelector').disabled = true;
	}else{
		//enable the item selector when a programme is selected
		$('itemSelector').disabled = false;
		//populate the item list
		ILpopulateItemList();
	}
	//automatically clears previous nodes on load by default
	//url from which to get a JSON response
	treeLoader.url = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgramme.pl';
	//on load get rootnode and expand all nodes
	treeLoader.load(tree.getRootNode(),function(){tree.expandAll();});
}

/// Open a printable version of the data
function ILprint () {
	if(typeof $('programmeSelector').options[$('programmeSelector').selectedIndex] == "undefined" || $('programmeSelector').options[$('programmeSelector').selectedIndex].value == -1) {
		Ext.MessageBox.alert('Cannot Print', 'Please select a series and programme before attempting to print.');
		return -1;
	}
	var series = $('seriesSelector').options[$('seriesSelector').selectedIndex].text;
	var programme = $('programmeSelector').options[$('programmeSelector').selectedIndex].text;
	var now = new Date();
	var monthname=new Array("Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec")
	var date = now.getDate()+" "+monthname[now.getMonth()]+" "+now.getFullYear();
	var newwindow=window.open('','ILprintWindow','height=400,width=980,scrollbars=1,resizable=1');
	var tmp = newwindow.document;
	tmp.write('<html><head><title>Ingex Log</title>');
	tmp.write('<link rel="stylesheet" href="/ingex/printpage.css">');
	tmp.write('</head><body><div id="progInfoDiv">');
	tmp.write('<div class="date">Date: '+date+'</div>');
	tmp.write('<div class="producer">Producer: '+$('producer').value+'</div>');
	tmp.write('<div class="director">Director: '+$('director').value+'</div>');
	tmp.write('<div class="pa">PA: '+$('pa').value+'</div></div>');
	tmp.write('<h1>'+series+'</h1>');
	tmp.write('<h2>'+programme+'</h2>');
	tmp.write('<div style="clear:both">&nbsp;</div>');
	tmp.write('<table><tr class="header"><th>TAKE</th><th colspan=2>TIMECODE</th><th>RESULT</th><th>COMMENT</th><th>LOCATION</th><th>DATE</th>');
	var items = tree.getRootNode().childNodes;
	for(var i in items) {
		if(typeof items[i].attributes != "undefined") {
			tmp.write('<tr class="itemHeaderLine"><td colspan=4></td><td class="itemHeader">'+items[i].attributes.itemName+'</td><td colspan=2></td></tr>');
			var takes = items[i].childNodes;
			for(var t in takes) {
				if (typeof takes[t].attributes != "undefined") {
					var a = takes[t].attributes;
					tmp.write('<tr>');
					tmp.write('<td class="take">'+a.takeNo+'</td>');
					tmp.write('<td class="tclabel">in:<br />out:<br />dur:</td>');
					tmp.write('<td class="timecode">'+a.inpoint+'<br />'+a.out+'<br />'+a.duration+'</td>');
					tmp.write('<td class="result">'+a.result+'</td>');
					tmp.write('<td class="comment">'+a.comment+'</td>');
					tmp.write('<td class="location">'+a.location+'</td>');
					tmp.write('<td class="date">'+a.date+'</td>');
					tmp.write('</tr>');
				}
			}
		}
	}
	tmp.write('</table></body></html>');
	tmp.close();
	newwindow.print();
}

/// Import a script (un-implemented)
function ILimport () {
	Ext.MessageBox.alert('Import Unavailable', 'Unfortunately, this feature has not yet been implemented.');
}

/// A "class" for a timecode
function ingexLoggingTimecode () {
	
	//holds the value of the currently selected
	var recSel = $('recorderSelector');
	
	//holds the current time
	this.h = 0;
	this.m = 0;
	this.s = 0;
	this.f = 0;
	//holds whether timecode is stuck (only from recorder)
	this.stopped = false;

	//date objects for start and stop points
	this.startDate;
	this.stopDate;
	
	//holds the duration
	this.durH = 0;
	this.durM = 0;
	this.durS = 0;
	this.durF = 0;
	//total framecount of duration
	this.durFtot = 0;
	
	//number of frames since midnight, at start, at end and currently
	this.firstFrame = 0;
	this.lastFrame = 0;
	this.framesSinceMidnight;
	
	//whether a take is running
	this.takeRunning = false;
	
	//holds an integer that counts down to zero and does a server update when is zero
	this.serverCheck = 0;

	//holds the time interval between incrementing
	this.incrementer = false;

	this.ajaxtimeout = false;

	//chooses the correct increment method based on selection
	this.incrementMethod = function(){
		//TODO is it necessary to prevent the change of recorder input whilst a take is running.
		
		//if no timecode supply selected do nothing
		if (recSel.options[recSel.selectedIndex].value == -1){
		 	clearInterval(ILtc.incrementer);
			ILtc.incrementer = false;
			clearTimeout(ILtc.ajaxtimeout);
			ILtc.ajaxtimeout = false;
			dropFrame = false;
			$('tcDisplay').innerHTML = "--:--:--:--";
			
		}else if (recSel.options[recSel.selectedIndex].value >= numOfRecs){ //if system time selected
			if($('tcDisplay').innerHTML == "--:--:--:--" ) {
					showLoading('tcDisplay');
			}
				
			//clear incrementer and begin updating from local system time
			clearInterval(ILtc.incrementer);
			ILtc.incrementer = false;
			clearTimeout(ILtc.ajaxtimeout);
			ILtc.ajaxtimeout = false;
			//insole.log("system time is selected with value ="+recSel.options[recSel.selectedIndex].value);
			
			if(recSel.options[recSel.selectedIndex].value == numOfRecs){//25fps
				frameNumer = 25;
				frameDenom = 1;
				editrate =  (frameNumer / frameDenom);
				dropFrame = false;
			}
			if(recSel.options[recSel.selectedIndex].value == numOfRecs + 1){//29.97 non-drop
				frameNumer = 30000;
				frameDenom = 1001;
				editrate =  (frameNumer / frameDenom);
				dropFrame = false;
			}
			if(recSel.options[recSel.selectedIndex].value == numOfRecs + 2){//29.97 drop
				frameNumer = 30000;
				frameDenom = 1001;
				editrate =  (frameNumer / frameDenom);
				dropFrame = true;
			}
			if(recSel.options[recSel.selectedIndex].value == numOfRecs + 3){//30 non-drop
				frameNumer = 30;
				frameDenom = 1;
				editrate =  (frameNumer / frameDenom);
				dropFrame = false;
			}
			
			ILtc.startIncrementing(false);//false for use system time
			
		}else { //if a recorder is selected
			if($('tcDisplay').innerHTML == "--:--:--:--") {
				showLoading('tcDisplay');
			}
			

			//insole.log("a recorder is selected with value ="+recSel.options[recSel.selectedIndex].value);
			clearInterval(ILtc.incrementer);
			ILtc.incrementer = false;
			clearTimeout(ILtc.ajaxtimeout);
			ILtc.ajaxtimeout = false;
			//make a request to update the edit rate			
			var xmlHttp = getxmlHttp();
			//insole.warn("created xmlhttp");
			xmlHttp.onreadystatechange = function () {
				//waits until response content is loaded before doing anything
				if (xmlHttp.readyState==4) {
					
					//insole.alert("have reached readystate 4 within method choice");
					if (ILtc.ajaxtimeout) {
						clearTimeout(ILtc.ajaxtimeout);
						//insole.warn("clearing timeout");
						ILtc.ajaxtimeout = false;
					}
				
					try {
						//insole.warn("trying to get response text");
						var tmp = JSON.parse(xmlHttp.responseText);
						var data = tmp.tc;
						ILtc.setEditrate(data.frameNumer,data.frameDenom);
						
					} catch (e) {
						clearInterval(ILtc.incrementer);
						insole.alert("Received invalid response when updating timecode (JSON parse error). Timecode display STOPPED.");
						insole.log("Exception thrown:   Name: "+e.name+"   Message: "+e.message);
						$('tcDisplay').innerHTML = "<span class='error'>Error</span>";
						Ext.MessageBox.alert('ERROR getting recorder URL!', 'Unable to connect to recorder : Please make sure Ingex is running and check status');
					}
					//calling start function
					ILtc.startRecIncrement();
				}
			};

			//only search for a recorder url if on the Logging tab
			if (currentTab == "Logging"){
				//var recSel = $('recorderSelector');
				
				if (recSel.options[recSel.selectedIndex].value != -1 && recSel.options[recSel.selectedIndex].value < numOfRecs ){  //if a recorder is selected
					//insole.warn("a recorder is selected and a request will be made");
					var recorderURL = ILgetRecorderURL(recSel.options[recSel.selectedIndex].text);
					
					if(!recorderURL){
						insole.alert("Error querying recorder for timecode - no recorder URL available");
						return false;
					}
			
					var now = new Date();
					var random = Math.floor(Math.random()*101);
					var requestID = now.getHours()+now.getMinutes()+now.getSeconds()+now.getMilliseconds()+random;
					//insole.warn("making a request to the server within method choice");
					//make a request to server
					xmlHttp.open("GET","/cgi-bin/ingex-modules/Logging.ingexmodule/tc.pl?url="+recorderURL+"&"+requestID,true);
					//insole.warn("after open edirate set to "+editrate);
					xmlHttp.send(null);
					//if this is set clear it
					if (this.ajaxtimeout) {
						clearTimeout(this.ajaxtimeout);
					}
					//if server doesn't respond in time then times out
					this.ajaxtimeout = setTimeout(function(){ILtc.ajaxTimedout()},5000);
				}
			}
			
		}
	}
	
	this.startRecIncrement = function(){
		ILtc.startIncrementing(true);//true for recorder selected
	}

	//updates the timecode from a given framecount
	this.updateTimecodeFromFrameCount = function(frameCount){
		
		
		var tcEditRate;
		if(editrate == ntscFps ){
			if(!dropFrame){
				tcEditRate = Math.round(editrate);//eg 29.97 is in base 30 timecode
			}else{tcEditRate = editrate;}
		}else{
			tcEditRate = editrate;
		}
		
		if (!dropFrame){
			//this is correct for any non drop frame rate 
			//insole.warn("fps = "+editrate);
			//insole.warn("framecount ="+frameCount+"edit rate="+editrate);
			this.s = Math.floor(frameCount/tcEditRate);
			this.m = Math.floor(this.s/60);
			this.h = Math.floor(this.m/60);
			//insole.warn("frameCount="+frameCount+" minus this"+(editrate * this.s));
			this.f = Math.floor(frameCount - (tcEditRate * this.s));
			this.s = this.s - (60 * this.m);
			this.m = this.m - (60 * this.h);
						
		}else{//is drop frame
			
			//if using system time or just incrementing then need to work out the timecode from framecount
			var countOfFrames = frameCount;
			this.h = Math.floor( frameCount / FRAMES_PER_DF_HOUR);
			countOfFrames = countOfFrames % FRAMES_PER_DF_HOUR;
			var ten_min = Math.floor(countOfFrames / FRAMES_PER_DF_TENMIN);
			countOfFrames = (countOfFrames % FRAMES_PER_DF_TENMIN);
			
			// must adjust frame count to make minutes calculation work
		        // calculations from here on in just assume that within the 10 minute cycle
			// there are only DF_FRAMES_PER_MINUTE (1798) frames per minute - even for the first minute
			// in the ten minute cycle. Hence we decrement the frame count by 2 to get the minutes count
			// So for the first two frames of the ten minute cycle we get a negative frames number

			countOfFrames -= 2;

			var unit_min = Math.floor(countOfFrames / FRAMES_PER_DF_MIN);
			countOfFrames = countOfFrames % FRAMES_PER_DF_MIN;
			this.m = (ten_min * 10) + unit_min;
			
			// frames now contains frame in minute @ 1798 frames per minute
	        	// put the 2 frame adjustment back in to get the correct frame count
	        	// For the first two frames of the ten minute cycle, frames is negative and this
	        	// adjustment makes it non-negative. For other minutes in the cycle the frames count
	        	// goes from 0 upwards, thus this adjusment gives the required 2 frame offset
			
			countOfFrames += 2;
	
		        this.s = Math.floor(countOfFrames / tcEditRate);
        		this.f = Math.floor(countOfFrames % tcEditRate);

		}
		//insole.warn("within update from count h= "+this.h+" m="+this.m+"  s="+this.s+"  f="+this.f);
		//insole.warn("frameCount="+frameCount+" minus this"+(editrate * this.s));
	}

	
	//returns number of frames since midnight from a passed date object
	this.getFramesSinceMidnightFromDate = function (dateObj){
		var h; var m; var s; var f;
		h = dateObj.getHours();
		m = dateObj.getMinutes();
		s = dateObj.getSeconds();
		f = Math.floor((editrate/1000)*dateObj.getMilliseconds());
		//returns the total number of frames since midnight
		return (Math.floor(h*60*60*editrate) + Math.floor(m*60*editrate) + Math.floor(s*editrate) + f);
	}	
	
	//starts incrementing the correct method
	this.startIncrementing = function(recorderSelected) {
		//if no incrementer set	
		//insole.warn("starting incrementing with this.incrementer="+this.incrementer+"  and recorder selected = "+recorderSelected);
		if(!this.incrementer) {
			if(recorderSelected){//getting update from server timecode
				
				//insole.warn("in startIncrementing editrate = "+editrate);
				
				
				//update timecode from server at start
				this.update();
				
				// Format as 2 digit numbers
				var h; var m; var s; var f;
				if (this.h < 10) { h = "0" + this.h; }else{h = this.h;};
				if (this.m < 10) { m = "0" + this.m; }else{m = this.m;}
				if (this.s < 10) { s = "0" + this.s; }else{s = this.s;}
				if (this.f < 10) { f = "0" + this.f; }else{f = this.f;}
				

				if (h==NaN || m==NaN || s==NaN){
					showLoading('tcDisplay');
				}else{
					//display to timcode display
					if (dropFrame){
						$('tcDisplay').innerHTML == h+";"+m+";"+s+";"+f;
					}else{
						$('tcDisplay').innerHTML == h+":"+m+":"+s+":"+f;
					}
				}
				//if the server update time is less than the increment time then no point setting incrementer just set a server update	
				if (serverUpdatePeriod < ( tcFreq*(1000/editrate))){
					this.incrementer = setInterval(function(){ILtc.update();},serverUpdatePeriod);
				}else{//if the server update time is longer than the increment period
					//set incrementer
					//number of increments before serverCheck
					this.serverCheck = Math.floor(serverUpdatePeriod /( tcFreq*(1000/editrate)));//every serverCheck increments will update from server instead of incrementing in browser.
					//insole.warn("initially setting serverCheck to"+this.serverCheck+"  recorderselected="+recorderSelected);
					//update the timecode by tcFreq frames every time 3 frames worth of time has passed
					this.incrementer = setInterval(function(){ILtc.increment(tcFreq, recorderSelected);},(tcFreq*(1000/editrate)));
					//insole.warn("else increment ="+this.incrementer);
				}
				
			}else {//getting update from local timecode
				//get current time	
				var now = new Date();
				//get frames since midnight for current date
				this.framesSinceMidnight = this.getFramesSinceMidnightFromDate(now);
				//update timecode from frames since midnight - auto detects dropframe
				this.updateTimecodeFromFrameCount(this.framesSinceMidnight);
				
				//set increment to update the timecode  approximately every time tcFreq frames worth of time has passed
				this.incrementer = setInterval(function(){ILtc.increment(tcFreq, recorderSelected);},(tcFreq*(1000/editrate)));
				//insole.warn("else increment ="+this.incrementer);
				//insole.warn("hours="+now.getHours()+":"+now.getMinutes());
				//insole.warn("this.h="+this.h+"   this.m = "+this.m);
			}
		}
	}
	
	
	//update the timecode from the server
	this.update = function() {
		//insole.warn("now updating from server");
		var xmlHttp = getxmlHttp();
		xmlHttp.onreadystatechange = function () {
			if (xmlHttp.readyState==4) {
				//insole.warn("timeout = "+ILtc.ajaxtimeout+"have reached readystate4");
				if (ILtc.ajaxtimeout) {
					clearTimeout(ILtc.ajaxtimeout);
					ILtc.ajaxtimeout = false;	
				}
				try {
					var tmp = JSON.parse(xmlHttp.responseText);
					var data = tmp.tc;
					ILtc.set(data.h,data.m,data.s,data.f,data.frameNumer, data.frameDenom,data.framesSinceMidnight,data.dropFrame,data.stopped,false);
					
				} catch (e) {
					clearInterval(ILtc.incrementer);
					insole.alert("Received invalid response when updating timecode (JSON parse error). Timecode display STOPPED.");
					insole.log("Exception thrown:   Name: "+e.name+"   Message: "+e.message);
					$('tcDisplay').innerHTML = "<span class='error'>Error</span>";
				}
			}
		};
		
		//only search for a recorder if on the Logging tab
		if (currentTab == "Logging"){
			//var recSel = $('recorderSelector');
			
			if (recSel.options[recSel.selectedIndex].value != -1 && recSel.options[recSel.selectedIndex].value < numOfRecs ){  //if a recorder is selected

				var recorderURL = ILgetRecorderURL(recSel.options[recSel.selectedIndex].text);
				
				if(!recorderURL){
					 insole.alert("Error querying recorder for timecode - no recorder URL available");
					return false;
				}
		
				var now = new Date();
				var random = Math.floor(Math.random()*101);
				var requestID = now.getHours()+now.getMinutes()+now.getSeconds()+now.getMilliseconds()+random;
				//insole.warn("making a request to the server within update");
				xmlHttp.open("GET","/cgi-bin/ingex-modules/Logging.ingexmodule/tc.pl?url="+recorderURL+"&"+requestID,true);
				xmlHttp.send(null);
				if (this.ajaxtimeout) {
					clearTimeout(this.ajaxtimeout);
				}
				this.ajaxtimeout = setTimeout(function(){ILtc.ajaxTimedout()},5000);
			}
		}

	}
	
	this.ajaxTimedout = function() {
		clearTimeout(this.ajaxtimeout);
		clearInterval(this.incrementer);
		this.ajaxtimeout = false;
		insole.alert("AJAX timeout occurred when updating timecode. Moving to system time");
		this.startIncrementing(false);
		$('tcDisplay').innerHTML = "<span class='error'>Error</span>";
	}

	this.setEditrate = function(numerFrame,denomFrame) {
		editrate = numerFrame/denomFrame;
	}

	//set the current timecode from server framecount
	this.set = function(h,m,s,f,numerFrame,denomFrame,framesSinceMidnight,frameDrop,stopped) {
		//update timecode
		this.h = h;
		this.m = m;
		this.s = s;
		this.f = f;
		frameNumer = numerFrame;
		frameDenom = denomFrame;
		editrate = frameNumer/frameDenom;
		//insole.warn("in set edit rate = "+editrate);
		//insole.warn("in set frameNUmer="+frameNumer);
		this.framesSinceMidnight  = framesSinceMidnight;
		dropFrame = frameDrop;
		this.stopped = stopped;
		
		//change the duration total to match updated timecode
		if (this.takeRunning){
			//work out the current duration
			this.lastFrame = this.framesSinceMidnight;
			if(this.lastFrame > this.firstFrame)//not crossing midnight
			{
				this.durFtot = this.lastFrame - this.firstFrame;
			}else{
				this.durFtot = (this.lastFrame + ((24*60*60*editrate) - this.firstFrame))
			}
		}
	}

	this.increment = function(numToInc, recorderSelected) {
		//if number of frames to increment by is not set then set to 1
		if (isDefault(numToInc)) numToInc = 1; //number of frames to increment by
		//insole.warn("this.h = "+this.h+"  this.m="+this.m);
		//insole.warn("incrementer has been called with recorderSelected ="+recorderSelected);
		if(recorderSelected){
			//insole.warn("a recorder is selected")
			if (this.serverCheck == 0){//do a server update
				//insole.warn("updating from server");
				//don't need to worry about dropframe as that is automatically provided by Timecode object
				this.update();
				//reset the serverIncrement
				this.serverCheck = Math.floor(serverUpdatePeriod /( tcFreq*(1000/editrate)));
				//insole.warn("servercheck reset to"+this.serverCheck);
			}else{
				//count down til next server check
				this.serverCheck --;
				//do the basic system time increment
				this.framesSinceMidnight +=numToInc;
				this.updateTimecodeFromFrameCount(this.framesSinceMidnight);
				//insole.warn("serverCheck currently"+this.serverCheck);
			}
		}else{//using system time
			
				//get current time
				var now = new Date();
				///update the frames since midnight which is used for incrementing
				this.framesSinceMidnight = this.getFramesSinceMidnightFromDate(now);
				this.updateTimecodeFromFrameCount(this.framesSinceMidnight);
			
		}
		
		if(this.takeRunning){
			//update lastFrame of take
			this.lastFrame = this.framesSinceMidnight;
			
		}
		//insole.warn("at first increment h= "+this.h+" m="+this.m+"  s="+this.s+"  f="+this.f);
		
		var fr = '--';
				
		// Format as 2 digit numbers
		var h; var m; var s; var f;
		if (this.h < 10) { h = "0" + this.h; }else{h = this.h;};
		if (this.m < 10) { m = "0" + this.m; }else{m = this.m;}
		if (this.s < 10) { s = "0" + this.s; }else{s = this.s;}
		if (this.f < 10) { f = "0" + this.f; }else{f = this.f;}
		
		if(this.f >= editrate){insole.alert("The number of frames is MORE THAN FPS");}
		if(this.f < 0){insole.alert("the number of frames is NEGATIVE");}
		//insole.warn("editrate beore tc display ="+editrate);
		//insole.warn("before tc display "+h+":"+m+":"+s+":"+f);
		// draw timecode to tcDisplay
		if(dropFrame){
			if (showFrames){ $('tcDisplay').innerHTML = h+";"+m+";"+s+";"+f;}
			else{ $('tcDisplay').innerHTML = h+";"+m+";"+s+";"+fr;}	
		}else{
			if (showFrames){ $('tcDisplay').innerHTML = h+":"+m+":"+s+":"+f;}
			else{ $('tcDisplay').innerHTML = h+":"+m+":"+s+":"+fr;}
		}
				
		if(this.takeRunning) {
			//update duration				
			//avoid odd behaviour if timecode crosses midnight
			if(this.lastFrame > this.firstFrame) //not crossing midnight
			{
				this.durFtot = this.lastFrame - this.firstFrame;
			}else{//add number of frames in a day to the last frame and then calculate
				this.durFtot = (this.lastFrame + ((24*60*60*editrate) - this.firstFrame))
			}

			//need to convert to timecode from total 
			//get duration timecode from number of frames
			this.durS = Math.floor(this.durFtot/editrate);
			//insole.warn("seconds total="+this.durS);
			this.durM = Math.floor(this.durS/60);
			this.durH = Math.floor(this.durM/60);
			this.durF = this.durFtot - (editrate * this.durS);
			this.durS = this.durS - (60 * this.durM);
			this.durM = this.durM - (60 * this.durH);
	
			//only needed if are adding to durF itself
			if (this.durF >= editrate) {
				insole.alert("*************** duration frame number more than editrate *************");
				this.durF -= editrate;
				this.durS++;
				if (this.durS > 59) {
					this.durS -= 59;
					this.durM++;
					if (this.durM > 59) {
						this.durM -= 59;
						this.durH++;
					}
				}
			}
			this.durF = Math.floor(this.durF);
			
			//insole.warn("duration update = "+this.durH+":"+this.durM+":"+this.durS+":"+this.durF);
			//drop frame alterations
			/*if (dropFrame){
			if(this.m % 10 != 0){//if recording time elapsed is  not divisible by ten, drop first 2 frames of each minute
				if(this.f == 0 && this.s == 0){this.f = 2;}//change displayed timecode to 'drop' frames
				if(this.f == 1 && this.s == 0){this.f = 3;}
				//don't increment the total duration
				//if(this.durF == 0 && this.durS == 0){this.durF = 2}
				//if(this.durF == 1 && this.durS == 0){this.durF = 3}
				}
			}*/
		
			// Format as 2 digit numbers
			var dh; var dm; var ds; var df;
			if (this.durH < 10) { dh = "0" + this.durH; } else { dh = this.durH; }
			if (this.durM < 10) { dm = "0" + this.durM; } else { dm = this.durM; }
			if (this.durS < 10) { ds = "0" + this.durS; } else { ds = this.durS; }
			if (this.durF < 10) { df = "0" + this.durF; } else { df = this.durF; }
			
			if(dropFrame){
				if(showFrames){
					$('outpointBox').value = h+ ";" + m + ";" + s + ";" + f;
					$('durationBox').value = dh + ":" + dm + ":" + ds + ":" + df;
				}else{
					$('outpointBox').value = h + ";" + m + ";" + s + ";"+ fr;
					$('durationBox').value = dh + ":" + dm + ":" + ds + ":" + fr;
				}
			}else{
				if(showFrames){
					$('outpointBox').value = h+ ":" + m + ":" + s + ":" + f;
					$('durationBox').value = dh + ":" + dm + ":" + ds + ":" + df;
				}else{
					$('outpointBox').value = h + ":" + m + ":" + s + ":"+ fr;
					$('durationBox').value = dh + ":" + dm + ":" + ds + ":" + fr;
				}
			}
		}
	}

	this.startTake = function(){
		//if no recorder is selected
		//insole.warn("starting take with selection");
		//get update from server if a recorder is selected
		if (recSel.options[recSel.selectedIndex].value < numOfRecs && recSel.options[recSel.selectedIndex].value != -1){
			//insole.warn("updating from server");
			ILtc.update();
			this.firstFrame = this.framesSinceMidnight;
			this.lastFrame = this.firstFrame;
		}else{ // system time is recorded
			this.startDate = new Date();
			this.firstFrame = ILtc.getFramesSinceMidnightFromDate(this.startDate);
			this.lastFrame = this.firstFrame;
		}
		
		ILtc.updateTimecodeFromFrameCount(this.firstFrame);
		
		//insole.warn("within startTake h= "+this.h+" m="+this.m+"  s="+this.s+"  f="+this.f);
		// Format as 2 digit numbers
		var h; var m; var s; var f;
		if (this.h < 10) { h = "0" + this.h; } else { h = this.h; }
		if (this.m < 10) { m = "0" + this.m; } else { m = this.m; }
		if (this.s < 10) { s = "0" + this.s; } else { s = this.s; }
		if (this.f < 10) { f = "0" + this.f; } else { f = this.f; }
		if(dropFrame){
			$('inpointBox').value = h + ";" + m + ";" + s + ";" + f;
			$('outpointBox').value = h + ";" + m + ";" + s + ";" + f;
		}else{
			$('inpointBox').value = h + ":" + m + ":" + s + ":" + f;
			$('outpointBox').value = h + ":" + m + ":" + s + ":" + f;
		}
		$('durationBox').value = "00:00:00:00";	
		this.durH = 0;
		this.durM = 0;
		this.durS = 0;
		this.durF = 0;
		ILtc.takeRunning = true;
		
		//disable the recorder selector whilst a take is running
		$('recorderSelector').disabled = true;

		//insole.warn("start take running="+ILtc.takeRunning);
	}

	this.stopTake = function(){
		//insole.warn("entering stopTake function");
		
		//if is a recorder
		if (recSel.options[recSel.selectedIndex].value < numOfRecs && recSel.options[recSel.selectedIndex].value != -1){
			ILtc.update();
			this.lastFrame = this.framesSinceMidnight;
		}else{
			this.stopDate = new Date();
			this.lastFrame = this.getFramesSinceMidnightFromDate(this.stopDate);
		}
		
		ILtc.takeRunning = false;
		//insole.warn("have set take running to "+ILtc.takeRunning)
		
		//work out the duration
		if(this.lastFrame > this.firstFrame)//not crossing midnight
		{
			this.durFtot = this.lastFrame - this.firstFrame;
		}else{// add frames in a day to last frame before doing calculation
			this.durFtot = (this.lastFrame + ((24*60*60*editrate) - this.firstFrame))
		}
		
		//insole.warn("updating timecode");
		//update this.h, this.m etc
		ILtc.updateTimecodeFromFrameCount(ILtc.lastFrame);
		
		//insole.warn("durFtot="+this.durFtot);
		//get duration time from number of frames
		this.durS = Math.floor(this.durFtot/editrate);
		//insole.warn("seconds total="+this.durS);
		this.durM = Math.floor(this.durS/60);
		this.durH = Math.floor(this.durM/60);
		this.durF = this.durFtot - (editrate * this.durS);
		this.durS = this.durS - (60 * this.durM);
		this.durM = this.durM - (60 * this.durH);
		this.durF = Math.floor(this.durF);
		
		//insole.warn("the duration"+this.durH+":"+this.durM+":"+this.durS+":"+this.durF);
		// Format as 2 digit numbers
		var h; var m; var s; var f;
		if (this.h < 10) { h = "0" + this.h; } else { h = this.h; }
		if (this.m < 10) { m = "0" + this.m; } else { m = this.m; }
		if (this.s < 10) { s = "0" + this.s; } else { s = this.s; }
		if (this.f < 10) { f = "0" + this.f; } else { f = this.f; }
		
		if(dropFrame){
			$('outpointBox').value = h + ";" + m + ";" + s + ";" + f;
		}else{
			$('outpointBox').value = h + ":" + m + ":" + s + ":" + f;
		}
		// Format as 2 digit numbers
		var dh; var dm; var ds; var df;
		if (this.durH < 10) { dh = "0" + this.durH; } else { dh = this.durH; }
		if (this.durM < 10) { dm = "0" + this.durM; } else { dm = this.durM; }
		if (this.durS < 10) { ds = "0" + this.durS; } else { ds = this.durS; }
		if (this.durF < 10) { df = "0" + this.durF; } else { df = this.durF; }
		$('durationBox').value = dh+":"+dm+":"+ds+":"+df;
		//insole.warn ("duration ="+this.durFtot+"  start count ="+this.firstFrame+"  endcount="+this.lastFrame);
		//enable the recorder selector
		$('recorderSelector').disabled = false;


	}
}


// --- Extending Ext library for Logging Module ---

Ext.tree.ColumnTree = Ext.extend(Ext.tree.TreePanel, {
    lines:false,
    borderWidth: Ext.isBorderBox ? 0 : 2, // the combined left/right border for each cell
    cls:'x-column-tree',

    onRender : function(){
        Ext.tree.ColumnTree.superclass.onRender.apply(this, arguments);
        this.headers = this.body.createChild(
            {cls:'x-tree-headers'},this.innerCt.dom);

        var cols = this.columns, c;
        var totalWidth = 0;

        for(var i = 0, len = cols.length; i < len; i++){
             c = cols[i];
             totalWidth += c.width;
             this.headers.createChild({
                 cls:'x-tree-hd ' + (c.cls?c.cls+'-hd':''),
                 cn: {
                     cls:'x-tree-hd-text',
                     html: c.header
                 },
                 style:'width:'+(c.width-this.borderWidth)+'px;'
             });
        }
        this.headers.createChild({cls:'x-clear'});
        // prevent floats from wrapping when clipped
        this.headers.setWidth(totalWidth);
        this.innerCt.setWidth(totalWidth);
    }
});

Ext.tree.TakeNodeUI = Ext.extend(Ext.tree.TreeNodeUI, {
    focus: Ext.emptyFn, // prevent odd scrolling behavior

    renderElements : function(n, a, targetNode, bulkRender){
        this.indentMarkup = n.parentNode ? n.parentNode.ui.getChildIndent() : '';

        var t = n.getOwnerTree();
        var cols = t.columns;
        var bw = t.borderWidth;
        var c = cols[0];

        var buf = [
             '<li class="x-tree-node"><div ext:tree-node-id="',n.id,'" class="x-tree-node-el x-tree-node-leaf ', a.cls,'">',
                '<div class="x-tree-col" style="width:',c.width-bw,'px;">',
                    '<span class="x-tree-node-indent">',this.indentMarkup,"</span>",
                    '<img src="', this.emptyIcon, '" class="x-tree-ec-icon x-tree-elbow">',
                    //'<img src="', a.icon || this.emptyIcon, '" class="x-tree-node-icon',(a.icon ? " x-tree-node-inline-icon" : ""),(a.iconCls ? " "+a.iconCls : ""),'" unselectable="on">',
					'<img src="/ingex/img/take.gif" unselectable="on" class="x-tree-node-icon" style="background-image:none;">',
                    '<a hidefocus="on" class="x-tree-node-anchor" href="',a.href ? a.href : "#",'" tabIndex="1" ',
                    a.hrefTarget ? ' target="'+a.hrefTarget+'"' : "", '>',
                    '<span unselectable="on">', n.text || (c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),"</span></a>",
                "</div>"];
         for(var i = 1, len = cols.length; i < len; i++){
             c = cols[i];

             buf.push('<div class="x-tree-col ',(c.cls?c.cls:''),'" style="width:',c.width-bw,'px;">',
                        '<div class="x-tree-col-text" dataIndex="',c.dataIndex,'">',(c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),"</div>",
                      "</div>");
         }
         buf.push(
            '<div class="x-clear"></div></div>',
            '<ul class="x-tree-node-ct" style="display:none;"></ul>',
            "</li>");

        if(bulkRender !== true && n.nextSibling && n.nextSibling.ui.getEl()){
            this.wrap = Ext.DomHelper.insertHtml("beforeBegin",
                                n.nextSibling.ui.getEl(), buf.join(""));
        }else{
            this.wrap = Ext.DomHelper.insertHtml("beforeEnd", targetNode, buf.join(""));
        }

        this.elNode = this.wrap.childNodes[0];
        this.ctNode = this.wrap.childNodes[1];
        var cs = this.elNode.firstChild.childNodes;
        this.indentNode = cs[0];
        this.ecNode = cs[1];
        this.iconNode = cs[2];
        this.anchor = cs[3];
        this.textNode = cs[3].firstChild;
		this.takeelements = new Object();
		this.takeelements['location'] = this.elNode.childNodes[1].firstChild;
		this.takeelements['date'] = this.elNode.childNodes[2].firstChild;
		this.takeelements['inpoint'] = this.elNode.childNodes[3].firstChild;
		this.takeelements['out'] = this.elNode.childNodes[4].firstChild;
		this.takeelements['duration'] = this.elNode.childNodes[5].firstChild;
		this.takeelements['result'] = this.elNode.childNodes[6].firstChild;
		this.takeelements['comment'] = this.elNode.childNodes[7].firstChild;
    },

	getTakeElement : function(name) {
		//insole.warn("name is="+name+"  takeelements is- "+this.takeelements.name);
		if(typeof this.takeelements[name] != "undefined") return this.takeelements[name];
		return false;
	}
});

Ext.tree.ItemNodeUI = Ext.extend(Ext.tree.TreeNodeUI, {
    focus: Ext.emptyFn, // prevent odd scrolling behavior

	getItemNameEl : function(){
		return this.itemNameNode;
	},
	
	getSequenceEl : function(){
		return this.sequenceNode;
	},

    renderElements : function(n, a, targetNode, bulkRender){
        this.indentMarkup = n.parentNode ? n.parentNode.ui.getChildIndent() : '';

        var t = n.getOwnerTree();
        var cols = t.columns;
        var bw = t.borderWidth;
        var c = cols[0];

        var buf = [
             '<li class="x-tree-node"><div ext:tree-node-id="',n.id,'" class="x-tree-node-el x-tree-node-leaf ', a.cls,'">',
                '<div class="x-tree-col" style="width:0px',c.width-bw,'px;">',
                    '<span class="x-tree-node-indent">',this.indentMarkup,"</span>",
                    '<img src="', this.emptyIcon, '" class="x-tree-ec-icon x-tree-elbow">',
                    // '<img src="', a.icon || this.emptyIcon, '" class="x-tree-node-icon',(a.icon ? " x-tree-node-inline-icon" : ""),(a.iconCls ? " "+a.iconCls : ""),'" unselectable="on">',
					'<img src="/ingex/img/item.gif" unselectable="on">',
                    '<a hidefocus="on" class="x-tree-node-anchor" href="',a.href ? a.href : "#",'" tabIndex="1" ',
                    a.hrefTarget ? ' target="'+a.hrefTarget+'"' : "", '>',
                    '<span unselectable="on">',/* n.text || (c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),*/"</span></a>",
                "</div>"];

            buf.push('<div class="x-tree-col" style="width:130px;border-bottom:1px solid #ddd;color: #449;">',
                        '<div class="x-tree-col-text" dataIndex="itemName">',a['itemName'],"</div>",
                      "</div>");

			var seq = "[no sequence]";
			if (a['sequence'] != "") seq = a['sequence'];
			buf.push('<div class="x-tree-col" style="width:80px;border-bottom:1px solid #ddd;color: #449;">',
                   '<div class="x-tree-col-text" dataIndex="sequence">',seq,"</div>",
                 "</div>");

         buf.push(
            '<div class="x-clear"></div></div>',
            '<ul class="x-tree-node-ct" style="display:none;"></ul>',
            "</li>");

        if(bulkRender !== true && n.nextSibling && n.nextSibling.ui.getEl()){
            this.wrap = Ext.DomHelper.insertHtml("beforeBegin",
                                n.nextSibling.ui.getEl(), buf.join(""));
        }else{
            this.wrap = Ext.DomHelper.insertHtml("beforeEnd", targetNode, buf.join(""));
        }

        this.elNode = this.wrap.childNodes[0];
        this.ctNode = this.wrap.childNodes[1];
        var cs = this.elNode.firstChild.childNodes;
        this.indentNode = cs[0];
        this.ecNode = cs[1];
        this.iconNode = cs[2];
        this.anchor = cs[3];
        this.textNode = cs[3].firstChild;
		this.itemNameNode = this.elNode.childNodes[1].firstChild;
		this.sequenceNode = this.elNode.childNodes[2].firstChild;
    }
});

/**
 * @class Ext.tree.ColumnTreeEditor
 * @extends Ext.Editor
 * Provides editor functionality for inline tree node editing.  Any valid {@link Ext.form.Field} subclass can be used
 * as the editor field.
 * @constructor
 * @param {TreePanel} tree
 * @param {Object} fieldConfig (optional) Either a prebuilt {@link Ext.form.Field} instance or a Field config object
 * that will be applied to the default field instance (defaults to a {@link Ext.form.TextField}).
 * @param {Object} config (optional) A TreeEditor config object
 */
Ext.tree.ColumnTreeEditor = function(tree, fc, config){
    fc = fc || {};
    var field = fc.events ? fc : new Ext.form.TextField(fc);
    Ext.tree.TreeEditor.superclass.constructor.call(this, field, config);

    this.tree = tree;

    if(!tree.rendered){
        tree.on('render', this.initEditor, this);
    }else{
        this.initEditor(tree);
    }
};

Ext.extend(Ext.tree.ColumnTreeEditor, Ext.Editor, {
    /**
     * @cfg {String} alignment
     * The position to align to (see {@link Ext.Element#alignTo} for more details, defaults to "l-l").
     */
    alignment: "l-l",
    // inherit
    autoSize: false,
    /**
     * @cfg {Boolean} hideEl
     * True to hide the bound element while the editor is displayed (defaults to false)
     */
    hideEl : false,
    /**
     * @cfg {String} cls
     * CSS class to apply to the editor (defaults to "x-small-editor x-tree-editor")
     */
    cls: "x-small-editor x-tree-editor",
    /**
     * @cfg {Boolean} shim
     * True to shim the editor if selects/iframes could be displayed beneath it (defaults to false)
     */
    shim:false,
    // inherit
    shadow:"frame",
    /**
     * @cfg {Number} maxWidth
     * The maximum width in pixels of the editor field (defaults to 250).  Note that if the maxWidth would exceed
     * the containing tree element's size, it will be automatically limited for you to the container width, taking
     * scroll and client offsets into account prior to each edit.
     */
    maxWidth: 250,
    /**
     * @cfg {Number} editDelay The number of milliseconds between clicks to register a double-click that will trigger
     * editing on the current node (defaults to 350).  If two clicks occur on the same node within this time span,
     * the editor for the node will display, otherwise it will be processed as a regular click.
     */
    editDelay : 350,

    initEditor : function(tree){
        tree.on('beforeclick', this.beforeNodeClick, this);
        tree.on('dblclick', this.onNodeDblClick, this);
        this.on('complete', this.updateNode, this);
        this.on('beforestartedit', this.fitToTree, this);
        //this.on('startedit', this.bindScroll, this, {delay:10});
        this.on('specialkey', this.onSpecialKey, this);
    },

    // private
    fitToTree : function(ed, el){
        var td = this.tree.getTreeEl().dom, nd = el.dom;
        if(td.scrollLeft >  nd.offsetLeft){ // ensure the node left point is visible
            td.scrollLeft = nd.offsetLeft;
        }
        var w = Math.min(
                this.maxWidth,
                (td.clientWidth > 20 ? td.clientWidth : td.offsetWidth) - Math.max(0, nd.offsetLeft-td.scrollLeft) - /*cushion*/5);
        this.setSize(w, '');
    },

    // private
    triggerEdit : function(node, defer, e){
        this.completeEdit();
		this.targetElement = e.getTarget();
		if(node.attributes.editable !== false && Ext.get(this.targetElement).hasClass('x-tree-col-text')){
	       /**
	        * The tree node this editor is bound to. Read-only.
	        * @type Ext.tree.TreeNode
	        * @property editNode
	        */
			this.editNode = node;
            if(this.tree.autoScroll){
                //node.ui.getEl().scrollIntoView(this.tree.body);
            }
            this.autoEditTimer = this.startEdit.defer(this.editDelay, this, [this.targetElement/*node.ui.textNode, node.text*/]);
            return false;
        }
    },

    // private
    bindScroll : function(){
        this.tree.getTreeEl().on('scroll', this.cancelEdit, this);
    },

    // private
    beforeNodeClick : function(node, e){
        clearTimeout(this.autoEditTimer);
        if(this.tree.getSelectionModel().isSelected(node)){
            e.stopEvent();
            return this.triggerEdit(node, null, e);
        }
    },

    onNodeDblClick : function(node, e){
        clearTimeout(this.autoEditTimer);
    },

    // private
	updateNode : function(ed, value){
		if(value == "") value = "[no data]";
		var dataIndex = this.targetElement.getAttribute('dataIndex')
		this.tree.getTreeEl().un('scroll', this.cancelEdit, this);
		var oldValue = this.editNode.attributes[dataIndex];
		//this.editNode.text = value;
		this.editNode.attributes[dataIndex] = value;
		if(this.editNode.rendered){ // event without subscribing
			this.targetElement.innerHTML = value;
		}
		this.editNode.fireEvent("valuechange", this.editNode, dataIndex, value, oldValue);
	},

    // private
    onHide : function(){
        Ext.tree.TreeEditor.superclass.onHide.call(this);
        if(this.editNode){
            this.editNode.ui.focus.defer(50, this.editNode.ui);
        }
    },

    // private
    onSpecialKey : function(field, e){
        var k = e.getKey();
        if(k == e.ESC){
            e.stopEvent();
            this.cancelEdit();
        }else if(k == e.ENTER && !e.hasModifier()){
            e.stopEvent();
            this.completeEdit();
        }
    }
});
