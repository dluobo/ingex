// Copyright (C) 2009  British Broadcasting Corporation
// Author: Sean Casey <seantc@users.sourceforge.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.


/*****************************************************************************************
 * Materials javascript
 * Main javascript class for materials page
 *****************************************************************************************/
 
// Show debug alerts?
var debug = false;

// ajax timeout period in milliseconds
var timeout_period = 600000;	// 10 mins	

var destRootNode = '';
var rootNode;
var userSettings;
var leafNodes = [];
var url;
var tree;
var dropBox;
var panel;
var fromDate;
var toDate;
var filterStyle;
var initialised = false;
var options = [];
var noSelectedItems = 0; 	// number of selected materials items
var destPkgSize = 0;		// size of package on destination tree
var selectedSrcNodes = ''; 	// currently selected nodes on source tree
var selectedDestNodes = '';	// currently selected nodes on destination tree
var toDelete = ''; 			// nodes flagged for deltion
var isLoading = false;
var multiResSelected = false;
var currentFormat = 0;			// current video format
var selectionFormat = 0;			// format of currently selected packages
var renderedExt;
var tip1;
var tip2;
var mode;					// view mode - single panel or drag/drop

var ONE_PANEL = 0;
var DRAG_DROP = 1;
var DEFAULT_MODE = 0;

var tree;
var dest_tree;

Ext.require([
    'Ext.data.*',
    'Ext.grid.*',
    'Ext.tree.*',
    'Ext.dd.*'
]);


var onLoad_Material = function() {
	Ext.onReady(function(){
		if(!isInitialised()){
			init();
			if ($('drag_drop')) { mode=DRAG_DROP;}
			else{ mode=DEFAULT_MODE; }
			
			// single panel or drag/drop mode:
		 	if(mode == DRAG_DROP){
		 		$('dnd_container').style.display="block";
		 	}
		 	else{
		 		$('dnd_container').style.display="none";
		 	}
			
			init_tree(mode);
		}
	});
}

// initialise
loaderFunctions.Material = onLoad_Material;

function init() {
	
	// user settings cookie object
	userSettings = new UserSettings(document);

	// from and to date pickers
	var today = new Date();
	
	fromDate = new Ext.form.DateField({
		renderTo: 'from_cal',
		name: 'date',
		width: '50px',
		value: today,
		format: 'd/m/y'
	});
	toDate = new Ext.form.DateField({
		renderTo: 'to_cal',
		name: 'date',
		width: '50px',
		value: today,
		format: 'd/m/y'
	});
	
	fromDate.disabled = 'true';
	toDate.disabled = 'true';
	
	// toggle buttons to select view mode
	Ext.create('Ext.Button', {
	    renderTo: 'loadclips_but',
	    text: 'Load Clips',
	    handler: submitFilter
	});
	Ext.create('Ext.Button', {
	    renderTo: 'createpkg_but',
	    text: 'Create Package',
	    handler: packageAAF
	});
	Ext.create('Ext.button.Split', {
        renderTo: 'viewmode_but', // the container id
        text: 'View',
        menu: new Ext.menu.Menu({
	        items: [
	                // these items will render as dropdown menu items when the arrow is clicked:
	                {text: 'Single Panel', data: ONE_PANEL, handler: viewmode_handler},
	                {text: 'Drag/Drop', data: DRAG_DROP, handler: viewmode_handler}
	        ]
        })
    });
	
	//get default export options
	//get settings from local cookie if stored
	var settings = loadDefaultExportSettings();
	
	// get len
	var len = 0;
	for(i in settings){len++}
	
	if(len){
		settings['fromcookie'] = 'TRUE';
		setExportDefaults(settings);
		updateEnabledOpts();
	}
	else{
		//no cookie stored - load global defaults from config file on web server instead
		loadExportOptions();
	}
}

function isInitialised() {
	if ($('tree').innerHTML) return 1;
	return 0;
}

function init_tree(mode) {
	// tooltips
	if (tip1) { tip1.destroy(); }
	if (tip2) { tip2.destroy(); }
	if(mode == DRAG_DROP){
		tip1 = new Ext.ToolTip({
			target:	'tree',
			html:	'Browse and select original material items from here'
		});
		tip2 = new Ext.ToolTip({
			target:	'dest_tree',
			html:	'Drag any materials you want to export into here'
		});
	}
	else{
		tip1 = new Ext.ToolTip({
			target:	'tree',
			html:	'Matching material items'
		});
	}
		
	Ext.define('LoaderModel',{
		extend: 'Ext.data.Model',
		fields: [
			{name: 'name_data', type: 'string'},
			{name: 'created_data', type: 'string'},
			{name: 'start_data', type: 'string'},
			{name: 'end_data', type: 'string'},
			{name: 'duration_data', type: 'string'},
			{name: 'format_data', type: 'string'},
			{name: 'tapeid_data', type: 'string'},
			
			{name: 'vresid', type: 'string'},
			{name: 'description', type: 'string'},
			{name: 'comments', type: 'string'},
			{name: 'iconCls', type: 'string'},
			{name: 'url', type: 'object'},
			{name: 'id', type: 'string'},
			{name: 'leaf', type: 'boolean'},
			{name: 'materialCount', type: 'int'},
			{name: 'materialIds', type: 'string'}
		],		
	});
		
    var store = Ext.create('Ext.data.TreeStore', {
        model: 'LoaderModel',
        proxy: {
            type: 'ajax',
            url: '/cgi-bin/ingex-modules/Material.ingexmodule/treeLoader.pl',
            extraParams: this.getLoaderParams() 
        },
        sorters: ['name_data', 'format_data']		// default sort fields
    });

	rootNode = Object({
		allowChildren: true,
        text: "Root node",
        id: '0',		//level depth
		name_data: 'All Projects',
        expanded: false
	});
	

    tree = Ext.create('Ext.tree.Panel', {
        width: 798,
        height: 298,
        containerScroll: true,
        preventHeader: true,
        border: false,
        renderTo: 'tree',
        collapsible: false,
        useArrows: true,
        rootVisible: true,
        store: store,
        multiSelect: true,
        singleExpand: true,
        root: rootNode,
        
        viewConfig: {
            plugins: {
                ptype: 'treeviewdragdrop',
                ddGroup: 'treedd',
                enableDrop: false,
                appendOnly: true
            }
        },
        listeners: {
        	click: {
	            element: 'el', //bind to the underlying el property on the panel
	            fn: materialClickedSrc
	        },
		},
        //the 'columns' property is now 'headers'
        columns: [{
            xtype: 'treecolumn', //this is so we know which column will show the tree
            text: 'Name',
            flex: 2,
            sortable: true,
            dataIndex: 'name_data'
        },{
            text: 'Created',
            flex: 1,
            sortable: true,
            dataIndex: 'created_data'
        },{
            text: 'Start',
            flex: 1,
            dataIndex: 'start_data',
            sortable: true
        },{
            text: 'End',
            flex: 1,
            dataIndex: 'end_data',
            sortable: true
        },{
            text: 'Duration',
            flex: 1,
            dataIndex: 'duration_data',
            sortable: true
        },{
            text: 'Format',
            flex: 1,
            dataIndex: 'format_data',
            sortable: true
        },{
            text: 'TapeID',
            flex: 1,
            dataIndex: 'tapeid_data',
            sortable: true
        }]
    });
	
	if(mode == DRAG_DROP){
			destRootNode = Object({
				allowChildren: true,
				text: 'Package',
				id: 'dbRoot',
				name: '0',
				name_data: 'Package',
				expanded: true
			});
			
			var store2 = Ext.create('Ext.data.TreeStore', {
		        model: 'LoaderModel',
		        proxy: {
		            type: 'ajax',
		            url: '/cgi-bin/ingex-modules/Material.ingexmodule/treeLoader.pl',
		            extraParams: this.getLoaderParams() 
		        },
		        sorters: ['name_data', 'format_data']		// default sort fields
		    });
	
			
	   		dest_tree = Ext.create('Ext.tree.Panel', {
	        	width: 798,
		        height: 298,
		        containerScroll: true,
		        preventHeader: true,
		        border: false,
		        renderTo: 'dest_tree',
		        collapsible: false,
		        useArrows: true,
		        rootVisible: true,
		        store: store2,
		        multiSelect: true,
		        singleExpand: true,
		        root: destRootNode,
		        viewConfig: {
		            
	        	},
	        listeners: {
			        	click: {
				            element: 'el', //bind to the underlying el property on the panel
				            fn: materialClickedDest
				        },
					},
		    
	        //the 'columns' property is now 'headers'
	        columns: [{
	            xtype: 'treecolumn', //this is so we know which column will show the tree
	            text: 'Name',
	            flex: 2,
	            sortable: true,
	            dataIndex: 'name_data'
	        },{
	            text: 'Created',
	            flex: 1,
	            sortable: true,
	            dataIndex: 'created_data'
	        },{
	            text: 'Start',
	            flex: 1,
	            dataIndex: 'start_data',
	            sortable: true
	        },{
	            text: 'End',
	            flex: 1,
	            dataIndex: 'end_data',
	            sortable: true
	        },{
	            text: 'Duration',
	            flex: 1,
	            dataIndex: 'duration_data',
	            sortable: true
	        },{
	            text: 'Format',
	            flex: 1,
	            dataIndex: 'format_data',
	            sortable: true
	        },{
	            text: 'TapeID',
	            flex: 1,
	            dataIndex: 'tapeid_data',
	            sortable: true
	        }]
	    });
	    
	    // Drop target listener
    	var panelDropTargetEl = dest_tree.body.dom;
    	
		var panelDropTarget = Ext.create('Ext.dd.DropTarget', panelDropTargetEl, {
			ddGroup: 'treedd',
			notifyEnter: function(ddsource, e, data){
				// add some style
				// dest_tree.body.stopFx();
				dest_tree.body.highlight();
			},
			notifyDrop: function(ddSource, e, data){
				copyNodesAndPaths(ddSource.dragData.records);	
			}
		});
	}
	
   	//refresh
    submitFilter();
}


 /*
  * apply a filter to the source nodes
  */
 function submitFilter(){
 	$('filter_matches').innerHTML = ''; //clear number of matching material items text
 	$('status_bar_src').innerHTML = '';	//clear currently selected node text 
 	
 	if(!formValid()){
 		//show an error
 		input_err('time_range_err');
 		return;
 	}

 	//form is valid
 	clear_input_errs();
 	
 	//collect form elements
 	options['projname'] = $('proj_name').value;
 	options['format'] = $('v_res').value;
 	
 	currentFormat = options['format']; 
 	currentFormat == -1 ? multiResSelected = true :	multiResSelected = false;
 	
 	if($('all_time_radio').checked){
 		//select all times
 		options['fromtime'] = 0;
 		options['totime'] = 0; 
 		
 		options['range'] = 0; 
 		options['day'] = 0; 
 	}
 	
 	else if($('time_period_radio').checked){
 		//select times in the time period->day
 		options['fromtime'] = 0;
 		options['totime'] = 0; 
 		
 		options['range'] = $('range_in').value;
 		options['day'] = $('day_in').value;
 	}
 	
 	else if($('time_range_radio').checked){
 		//select times from startdate->enddate
 		options['fromtime'] = getFromTime();	//unix timestamp 
 		options['totime'] = getToTime();
 		
 		options['range'] = 0; 
 		options['day'] = 0; 
 	
 	}
 	
 	options['searchtext'] = $('search_text').value;	//space separated keywords
 	
 	countPackages(options);
 	refreshTree(options);
 }


/*
 * Get parameters to send with treeLoader URL
 */
function getLoaderParams(){
	var params = Object({
		isIngexWeb: 1,
		dayIn: 0,
		formatIn: -1,
		keywordsIn: '',
		projectIn: -1,
		rangeIn: 0,
		tEndIn: 0,
		tStratIn: 0,
						
 		formatIn: options['format'],
 		tStartIn: options['fromtime'],
		tEndIn: options['totime'],
		keywordsIn: options['searchtext'],
		projectIn: options['projname'],
		rangeIn: options['range'],
		dayIn: options['day']	
	});
	return params;
}


 /*
  * reload tree data, using selected filter options
  */
function refreshTree(options){
 	currentFormat = options['format']; 
 	
 	tree.getRootNode().removeAll();
 	loaderParams = this.getLoaderParams();
	tree.store.proxy.extraParams = loaderParams;
	tree.root = rootNode;
	tree.getStore().load(); //reload the root
	tree.getRootNode().expand();	//expand the root
	
	if(mode == DRAG_DROP){
		dest_tree.store.proxy.extraParams = loaderParams;
		dest_tree.getRootNode().expand();
	}
}
 
 
function viewmode_handler(b, e){
 	mode = b.data;
 	
 	// clean up old Ext objs
 	Ext.destroy(tree);
 	Ext.destroy(dest_tree);
 	
 	$('tree').innerHTML = '';

 	// single panel or drag/drop mode:
 	if(mode == DRAG_DROP){
 		$('dnd_container').style.display="block";
 	}
 	else{
 		$('dnd_container').style.display="none";
 	}
 	
 	init_tree(mode);
}
 
 
// ext override, to modify timeout value
// Ext.tree.TreeLoader.override({
    // requestData : function(node, callback){
        // if(this.fireEvent("beforeload", this, node, callback) !== false){
            // this.transId = Ext.Ajax.request({
                // method:this.requestMethod,
                // url: this.dataUrl||this.url,
                // success: this.handleResponse,
                // failure: this.handleFailure,
                // timeout: this.timeout || 300000,	// 5 mins
                // scope: this,
                // argument: {callback: callback, node: node},
                // params: this.getParams(node)
            // });
        // }else{
            // // if the load is cancelled, make sure we notify
            // // the node that we are done
            // if(typeof callback == "function"){
                // callback();
            // }
        // }
    // }
// }); 
	
// set ext spacer image to a local file instead of remote url
// Ext.BLANK_IMAGE_URL = "../ingex/ext/resources/images/default/tree/s.gif";


/*
 * material item clicked on source tree
 */
function materialClickedSrc(node, e){materialClicked(node, e, 'src');}


/*
 * material item clicked on destination tree
 */
function materialClickedDest(node, e){materialClicked(node, e, 'dest');}


/*
 * called when a material item is clicked
 */
function materialClicked(node, e, elId){
	var singleNode = false;
	
	//store node
	if(elId == 'src'){
		selectedSrcNodes = removeDuplicateSiblings(tree.getSelectionModel().getSelection());
		if(selectedSrcNodes.length == 1){
			singleNode = true;
			node = selectedSrcNodes[0];
		}
	}
	else if(elId == 'dest'){
		selectedDestNodes = removeDuplicateSiblings(dest_tree.getSelectionModel().getSelection());
		if(selectedDestNodes.length == 1){
			singleNode = true;
			node = selectedDestNodes[0];
		}
	}
	
	if(singleNode && node && node.get('leaf')){
		noSelectedItems = 1;	//only 1 node selected
		
		var el = Ext.getDom('m_text');
		url = node.get('url');
		
		var trackLinks = '';
		var noTracks = 0;
		
		urls = node.get('url');
		for (var key in urls){
			var trackName = key;
			var url = urls[key];
			trackLinks = trackLinks + createTrackLink(trackName, url);
			noTracks++;
		}
		
		
		if(elId == 'src'){
			document.getElementById('package_src').style.visibility = 'visible';
			document.getElementById('tracks_src').innerHTML = trackLinks;
			
			var comments = node.get('comments');
			document.getElementById('meta_comments_src').innerHTML = comments;
			
			var description = node.get('description');
			document.getElementById('meta_description_src').innerHTML = description;
		
			//show size of selected package
 			document.getElementById('status_bar_src').innerHTML = '"' + node.get('name_data') + '" selected, containing ' + noTracks + ' tracks';
	
		}
		else{
			
//			// document.getElementById('package_dest').style.visibility = 'visible';
//			document.getElementById('tracks_dest').innerHTML = trackLinks;
//			
//			var comments = node.get('comments');
//			document.getElementById('meta_comments_dest').innerHTML = comments;
//			
//			var description = node.get('description');
//			document.getElementById('meta_description_dest').innerHTML = description;
//		
//			//show size of selected package
// 			document.getElementById('status_bar_dest').innerHTML = '"' + node.get('name_data') + '" selected, containing ' + noTracks + ' tracks';
	
		}
		
	}
	else{
		
		//show node size if on source tree
		if(elId == 'src'){
			var size = 0;
			
			if(selectedSrcNodes[0]){
				for(var i in selectedSrcNodes){
					if(selectedSrcNodes[i]){
						size += countPath(selectedSrcNodes[i], elId);
					}
				}
			}
			
			//only show size of selected nodes on source tree 
			setSrcPkgSize(size);
			document.getElementById('package_src').style.visibility = 'hidden';
		}
		
		//show nothing if on destination tree - we always want to show pkg size
		else if(elId == 'dest'){
//			var size = 0;
//			
//			if(selectedDestNodes[0]){
//				for(var i in selectedDestNodes){
//					if(selectedDestNodes[i]){
//						size += countPath(selectedDestNodes[i], elId);
//					}
//				}
//			}
//			
//			//only show size of selected nodes on source tree 
//			setDestPkgSize(size);
//			document.getElementById('package_dest').style.visibility = 'hidden';
		}
	}
}
	

/*
 * returns the number of material items on this path
 */
function countPath(node, elId){
	var pathSize;
	if(node.get('leaf')){pathSize = 1;}
	else{pathSize = node.get('materialCount');}
	return pathSize;
}


/*
 * get the top level parent of a node (1 level below root) 
 */
function getParent(n){
	var prev = '';
    var next = n;
        
    for(var i=0; true; i++){
                
    	if(next == rootNode){
 	       //stop - reached top level
 	       console.log(prev);
           return prev;
        }
          
        var copy = new Object();
		// copy.data.name_data = next.data.name_data;
		Ext.apply(Ext.copyTo(copy, next.data, 'name_data,created_data,start_data,end_data,duration_data,format_data,tapeid_data,vresid,description,comments,iconCls,url,id,leaf,materialCount,materialIds'));
		
		// var copy = next.copy();
		
        if(i>0){
        	copy.children = [];
			copy.children.push(prev);
		}
        prev = copy;
		var next = next.parentNode;
	}
}



/*
 * create a copy of a node and it's children
 */
function nodeCopy(n){
	var child = n.firstChild;
	var copy;
	
	if(!n.leaf && !child){
		var copy = new Object();
		Ext.apply(Ext.copyTo(copy, n.data, 'name_data,created_data,start_data,end_data,duration_data,format_data,tapeid_data,vresid,description,comments,iconCls,url,id,leaf,materialCount,materialIds'));
		
		copy.full = true;	//indicates that this path contains all the original materials
	}
	else{
		var copy = new Object();
		Ext.apply(Ext.copyTo(copy, n.data, 'name_data,created_data,start_data,end_data,duration_data,format_data,tapeid_data,vresid,description,comments,iconCls,url,id,leaf,materialCount,materialIds'));
	}
	if(child != null){
		var childCopy = nodeCopy(child);	//recursive copy
	}
	
	return copy;	//return the copy
}


/*
 * generate a graphical html link to a track url
 */
function createTrackLink(trackName, url){
	//add soft hyphens to url, to ensure it can be split
	var splitUrl = '';
	var next;
	var out;
	var av = trackName.substring(0,1); //audio or video
	
	out = '<a href=/cgi-bin/ingex-modules/Material.ingexmodule/download.pl?fileIn=' + url + '><div class=track_link_' + av + '>' + trackName + '</div></a>';
	return out;
}


/*
 * takes in node array and removes any nodes that are nth generation children of any other nodes in the array
 * to avoid repetition
 */
function removeDuplicateSiblings(nodes){
	var ret = [];
	ret[0] = nodes[0];
	var j=0;
	
	for(var i=1; i<nodes.length; i++){
		var next = nodes[i];
		var prev = ret[j];
		
		//only keep nodes that are not children of previously encountered ones
		if(!next.isAncestor(prev)){
			j++;
			ret[j] = next;
		}
	}
	
	return ret;
}

 
/*
 * see where node 'source' should fit into tree 'dest'
 * 
 * algorithm knows that source node should be 'full'
 * it looks for a point in the destination where the node should sit
 * 
 * INPUT: 
 * A source root tree node
 * A destination root tree node
 * 
 * OUTPUT:
 * A source node containing changes that needs to be inserted
 * A destination point at which the node fits
 * 
 */
function getNodeInsertPoint(source, dest){
	if(!dest.hasChildNodes()){return [source, dest];} //no child nodes exist 
	
	var children = dest.childNodes;
	
	if(debug){alert("looking for " + source.name_data + " in " + dest.get('name_data'));}
	console.log("looking for " + source.name_data + " in " + dest.get('name_data'));
	
	for(var i=0; i<children.length; i++){
		var child = children[i];
		if (!child){break;}	//ensures no null values are copied
		
		if(child.get('name_data') == source.name_data){
			//found it!
			if(child.get('full')){
				//already full - do not copy!
				return [-1,-1];
			}
			else{
				//check child node
				if(source.children.length > 0 && source.children[0]){
					var val = getNodeInsertPoint(source.children[0], child);
					if(val){return val;}
				}
			}
		}
	}
	
	//no matched folder names here...
	//copy to this path
	return [source, dest];
}


/*
 * check that multiple video resolutions are not contained in the dragged nodes and that
 * the resolution matches the packages currently in the selection
 */
function areNodesValid(nodes){
	// check they are all same res
	if(multiResSelected){
		return false;
	}
	
	//check this res matches any currently selected nodes
	if(destPkgSize > 0 && currentFormat != selectionFormat){
		return false;
	}
	
	return true;
}


/*
 * takes array of selected nodes and copies them to the destination tree
 * also updates node count for destination tree
 */
function copyNodesAndPaths(nodes){
	
	if(debug){alert('have '+nodes.length+' nodes');}
	
	if(!areNodesValid(nodes)){
		show_static_messagebox("Invalid Selection", "The package selection should contain only one video format. Select the desired format from the search options.");
		return;
	}
	
	for(var i=0; nodes.length; i++){
		console.log(nodes[i]);
		if(debug){alert('next node');}
		
		if (!nodes[i]){break;}	//ensures no null values are copied
		
		//if root node has been selected, copy all it's child nodes (the project names)
		if(nodes[i].getDepth() == 0){
			nodes = rootNode.childNodes;
		}
		
		var n = nodes[i];
		
		//this is a leaf - do not allow adding of single leaves
		if(n.get('leaf')){
			show_static_messagebox("Invalid Selection", "Individual packages cannot be selected. Select the whole multicamera group.");
			return;
		}
	
		//keep the path
		var node = getParent(n);
		
		if(debug){alert("top level parent: " + node.get('name'));}
		
		var sourceNode = node;	//start at project name
		var destNode = dest_tree.getRootNode();			//start at root 
		
		//now see where the path fits within the destination tree
		var ret = getNodeInsertPoint(sourceNode, destNode);
		console.log(ret);
		
		sourceNode = ret[0];
		nodePos = ret[1];
		if(!nodePos){nodePos = destNode;}
		if(debug){alert("insert point for node: " + nodePos);}
		console.log('insert point for node: '+nodePos.get('name_data'));
	
		//matched source path with destination - do not copy -
		if(nodePos == -1){
			
		}
		
		//no match found for nodeName, so insert node as child at this level
		else{
			var copy = sourceNode;
			var nodeName = copy.name_data;
			console.log('looking for '+nodeName+' on destination');
			
			//if this node already exists at the destination, remove it so it can be replaced with the new one
			if(nodePos.findChild('name_data', nodeName)){
				//get child
				var child = nodePos.findChild('name_data', nodeName, true);
				console.log(child);
				
				if(debug){alert(child.get('name_data'));}
				//deduct the nodes it contains
				var materialCount = getMaterialCount(child);
				destPkgSize -= materialCount;
				if(debug){alert("removing " + materialCount + " items");}
				
				//remove it
				nodePos.removeChild(child);
				
				//update 
 				document.getElementById('status_bar_dest').innerHTML = "Output package contains " + destPkgSize + " material items<br>";
 				
 				if(debug){alert("new dest pkg size: " + destPkgSize);}
			}
			
			//add the new node
			nodePos.appendChild(copy);

			//now add nodes to the total node count
			
			//this is a folder
			
			//add folder size to total count
			destPkgSize += countPath(n, 'dest');
		}
	}
	
	// //update destination package count
	setDestPkgSize(destPkgSize);
	
	//set root node to count
	destRootNode = dest_tree.getRootNode();
	destRootNode.materialCount = destPkgSize;
	
	//the current video format
	selectionFormat = currentFormat;
}

 
/*
 * create an avid or final cut package using materials in destination tree
 */
function packageAAF(){
	if(multiResSelected){
		//do not allow user to export as > 1 resolution in selection
		show_static_messagebox("Invalid Selection", "The package selection contains more than one video format. Select the desired format from the search options.");
		return;
	}
	
	show_wait_messagebox("Packaging Materials", "Please wait...");
	leafNodes = [];
	
	rootNode = tree.getRootNode();
	
	if(mode == DRAG_DROP){
		destRootNode = dest_tree.getRootNode();
		getLeaves(destRootNode);
	}	//drag/drop - load only nodes that have been dragged into selction tree
	else{
		getLeaves(rootNode);
	}	//not drag/drop - get all loaded nodes

	//got all leaves - execute create_aaf
	loadComplete();	
}


/*
 * get all leaf nodes in destination tree
 */
function getLeaves(node){
	
	 if(mode == DRAG_DROP){
		 if(node.getDepth() > 0){	//check not root node
			 
			 // this is a folder with unloaded children
			 if(!node.get('leaf') && !node.isLoaded()){
                 insole.log('unloaded folder');
				 
                 //there are unloaded children, so this path will contain all original materials
				 var materialIdsStr = node.get('materialIds');
				 var materialIds = materialIdsStr.split(",");
 				
				 for(var i=0; i<materialIds.length; i++)
				 {
					 leafNodes.push(materialIds[i]);
				 }
			 }
			 
			//this is a loaded folder, which contians some children
             else if(!node.get('leaf') && node.isLoaded()){

			     //investigate each child
			     var children = node.childNodes;
 
			     for(var i=0; i<children.length; i++)
                 {                 
				     if(children[i]){
					     getLeaves(children[i]);
                     }
                 }
 
                 return;
             }
		 }
		 else if(node.get('leaf') && node.get('id')>0){
			 //this is a material leaf
			 leafNodes.push(node.get('id'));
			 return;
		 }
		 else{
			 //this is a loaded folder, which contians some children
 		
			 //investigate each child
			 var children = node.childNodes;
 			
			 for(var i=0; i<children.length; i++)
			 {
				 if(children[i]){
					 getLeaves(children[i]);
				 }
			 }
 			
			 return;
		 }
	 }
	
	//non-dnd mode
	else{
		if(node.getDepth() == 0){	//is root node
			//investigate each project node
			var children = node.childNodes;
			
			for(var i=0; i<children.length; i++)
			{
				if(children[i]){
					getLeaves(children[i]);
				}
			}
			
			return;
		}
		else{	//get ids from each 'project' node
			var materialIdsStr = node.get('materialIds');
			var materialIds = materialIdsStr.split(",");
				//alert("getting " + materialIdsStr);	
			for(var i=0; i<materialIds.length; i++)
			{
				leafNodes.push(materialIds[i]);
			}
		}
	}
}

  
/*
 * loaded all leaves from tree - continue
 */
function loadComplete(){
	//check if user wants to save export options as defaults
	var saveOpts = document.getElementById('save').checked;	//save as defaults?
	
	if(saveOpts == true){
		var response = confirm("These export settings will be used as defaults on this computer. Continue?");
		
		if(response){
			//yes - continue
		}
		else{
			//do nothing
			return;
		}
	}
	
	if(debug){alert("load complete!");}
	var clipPkg = [];
	var j=0;
	
	
	// create JSON settings for create_aaf script
	
	// get leaf node parameters
	for(var i in leafNodes){
		if(leafNodes[i]>0){clipPkg[j] = {"PkgID":leafNodes[i]}; j++;}
	}

	// get export parameters
	var format = document.getElementById('format').value ;
	var fcp;
	if (format == 'fcp') {fcp = "TRUE";}
		else {fcp = "FALSE";}
	if (format == 'pdf') {pdf = "TRUE";}
		else {pdf = "FALSE";}
	var dircut = $('dircut').checked.toString().toUpperCase();
	var dircutaudio = $('dircutaudio').checked.toString().toUpperCase();
	var exportdir = $('exportdir').value;
	var fnameprefix = $('fnameprefix').value;
	var longsuffix = $('longsuffix').checked.toString().toUpperCase();
	var editpath = $('editpath').value;
	var dirsource = ''; 	
	
	// pdf log sheet mode
	if(pdf == "TRUE"){
		var json = 	{"Root": 
						[{
							"ApplicationSettings": 
								[{"FCP": fcp, "PDF": pdf}],
							"ClipSettings": clipPkg
						}]
					};
		
		var jsonText = JSON.stringify(json);
		saveDefaultExportSettings(json.Root[0].ApplicationSettings[0]);
		createPDF(jsonText);	// call script to create the PDF
	}
	
	// aaf for fcp package mode
	else{
		var json = 	{"Root": 
						[{
							"ApplicationSettings": 
								[{"FnamePrefix": fnameprefix, "ExportDir": exportdir, "FCP": fcp, "PDF": pdf, "LongSuffix": longsuffix, "EditPath": editpath, "DirCut": dircut, "DirSource": dirsource, "AudioEdit": dircutaudio}],
							"ClipSettings": clipPkg,
							"RunSettings":
								[{"GroupOnly": "TRUE", "Group": "TRUE", "MultiCam": "TRUE", "NTSC":"FALSE", "Verbose":"FALSE", "DNS":"", "User":"", "Password":""}]
						}]
					};
					
					
		var jsonText = JSON.stringify(json);
		
		if(saveOpts);
		saveDefaultExportSettings(json.Root[0].ApplicationSettings[0]);
		createAAF(jsonText);	// call script to create the package
	}
					
}


/*
 * save export settings locally in web cookie
 */
function saveDefaultExportSettings(settings){
	var val;
	
	for(var key in settings){
		val = settings[key];
		userSettings.set(key, val);
	}
	
	userSettings.save();
}


/*
 * load export settings locally from web cookie
 */
function loadDefaultExportSettings(){
	//user settings cookie object
	var params = userSettings.getAll();
	return params;
}


/*****************************************************************************************
 * enable/disable relevant html form elements
 */
 
// aaf export options
function updateEnabledOpts(){
	var format = $('format').value;
	var fcp;
	if (format == 'fcp') 
	{
		$('editpath').disabled = false;  //final cut pro selected
	}
	else 
	{
		$('editpath').disabled = true;
	}
	
	var dircut = $('dircut').checked;
	if (dircut)
	{
//		$('dirsource').disabled = false;
		$('dircutaudio').disabled = false;
	} 
	else
	{
//		$('dirsource').disabled = true;
		$('dircutaudio').disabled = true;
	}
}

// all times
function allTimesSet(){
	filterStyle = 'all';
	
	fromDate.disabled = true;
	toDate.disabled = true;
	
	$('hh_from').disabled = true;
	$('mm_from').disabled = true;
	$('ss_from').disabled = true;
	$('ff_from').disabled = true;
	
	$('hh_to').disabled = true;
	$('mm_to').disabled = true;
	$('ss_to').disabled = true;
	$('ff_to').disabled = true;
	
	$('range_in').disabled = true;
	$('day_in').disabled = true;
}

// enable day/period fields
function dayFilterSet(){
	filterStyle = 'day';
	
	fromDate.disabled = true;
	toDate.disabled = true;
	
	$('hh_from').disabled = true;
	$('mm_from').disabled = true;
	$('ss_from').disabled = true;
	$('ff_from').disabled = true;
	
	$('hh_to').disabled = true;
	$('mm_to').disabled = true;
	$('ss_to').disabled = true;
	$('ff_to').disabled = true;
	
	$('range_in').disabled = false;
	$('day_in').disabled = false;
}


// enable start/end date and time fields
function dateFilterSet(){
	filterStyle = 'date';
	
	fromDate.disabled = false;
	toDate.disabled = false;
	
	$('hh_from').disabled = false;
	$('mm_from').disabled = false;
	$('ss_from').disabled = false;
	$('ff_from').disabled = false;
	
	$('hh_to').disabled = false;
	$('mm_to').disabled = false;
	$('ss_to').disabled = false;
	$('ff_to').disabled = false;
	
	$('range_in').disabled = true;
	$('day_in').disabled = true;
}
/*
 * end: enable/disable relevant html form elements
 *****************************************************************************************/

/*
 * Get timestamp for 'time from' period
 */
function getFromTime(){
	var time = fromDate.getValue().getTime();
 	
 	var hh = parseInt($('hh_from').value, 10);
 	var mm = parseInt($('mm_from').value, 10);
 	var ss = parseInt($('ss_from').value, 10);
 	var ff = parseInt($('ff_from').value, 10);	//frame no 0-24
 	
 	time += ((ff / 25) * 1000)
 		 +  (ss * 1000)
 		 +  (mm * 60 * 1000)
 		 +  (hh * 60 * 60 * 1000);
 
	return time;
}
 

/*
 * Get timestamp for 'time to' period
 */
function getToTime(){
	var time = toDate.getValue().getTime();
	
 	var hh = parseInt($('hh_to').value, 10);
 	var mm = parseInt($('mm_to').value, 10);
 	var ss = parseInt($('ss_to').value, 10);
 	var ff = parseInt($('ff_to').value, 10);
 	
	time += ((ff / 25) * 1000)
 		 +  (ss * 1000)
 		 +  (mm * 60 * 1000)
 		 +  (hh * 60 * 60 * 1000);
 
	return time;
}
 

 /*
  * display output materials size
  */
 function setDestPkgSize(size){
 	if(debug){alert('setdestpkgsize');}
 	//update 
 	$('status_bar_dest').innerHTML = "Output package contains " + size + " material items<br>";
 }
 
 
  /*
   * display selected source materials size
   */
 function setSrcPkgSize(size){
 		//show size of selected folder
	 	if(selectedSrcNodes.length == 1){	//only one node selected
	 		if(selectedSrcNodes[0]){
		 		$('status_bar_src').innerHTML = selectedSrcNodes[0].get('name_data') + ' selected, containing ' + size + ' material items';
	 		}
	 	}
	 	else{	//multiple folders
	 		$('status_bar_src').innerHTML = 'Multiple items selected, containing ' + size + ' material items';
	 	}
 }
 

 
 /*
  * called to notify user of an error in form input
  */
 function input_err(item){
 	$(item).style.visibility = 'visible';
 }
 
 
 /*
  * hide all input errors
  */
 function clear_input_errs(){
 	$('time_range_err').style.visibility = 'hidden';
 }
 
 
 /*
  * check validity of form elements
  */
 function formValid(){

	//check validity of time/date range fields
 	if($('time_range_radio').checked){
 		//date validity is ensured by ext api
 		
 		//time validity
 		var hh = parseInt($('hh_from').value) 	//is an int?
 		if(hh > 23 || hh < 0 || isNaN(hh)){return false;}
 
 		var mm = parseInt($('mm_from').value) 	//is an int?
 		if(mm > 59 || mm < 0 || isNaN(mm)){return false;}
 		
 		var ss = parseInt($('ss_from').value) 	//is an int?
 		if(ss > 59 || ss < 0 || isNaN(ss)){return false;}
 		
 		var ff = parseInt($('ff_from').value) 	//is an int?
 		if(ff > 24 || ff < 0 || isNaN(ff)){return false;}
 		
 		var hh = parseInt($('hh_to').value) 	//is an int?
 		if(hh > 23 || hh < 0 || isNaN(hh)){return false;}
 
 		var mm = parseInt($('mm_to').value) 	//is an int?
 		if(mm > 59 || mm < 0 || isNaN(mm)){return false;}
 		
 		var ss = parseInt($('ss_to').value) 	//is an int?
 		if(ss > 59 || ss < 0 || isNaN(ss)){return false;}
 		
 		var ff = parseInt($('ff_to').value) 	//is an int?
 		if(ff > 24 || ff < 0 || isNaN(ff)){return false;}
 		
 	}
 	
 	return true;
 }
 
 
 /*
  * Set aaf export options to the supplied json settings
  */
 function setExportDefaults(json){
 	
 	//cookie derived
 	if(json['fromcookie'] === 'TRUE'){
 	 	if(json['FCP'] === 'TRUE'){$('format').value = 'fcp';}
		else if(json['PDF'] === 'TRUE'){$('format').value = 'pdf';}
		else {$('format').value = 'avid';}
		$('dircut').checked = (json['DirCut'] === 'TRUE');
		$('dircutaudio').checked = (json['AudioEdit'] === 'TRUE');
		$('exportdir').selectedIndex = json['ExportDir'];
		$('fnameprefix').value = json['FnamePrefix'];
		$('longsuffix').checked = (json['LongSuffix'] === 'TRUE');
		$('editpath').value = json['EditPath'];
//		$('dirsource').value = json['DirSource'];
 	}
 	
 	//server derived (json elements are object based)
 	else{
 		//set aaf export options to saved defaults
		$('format').value = json.FCP;
		$('dircut').checked = (json.DirCut === 'true');
		$('dircutaudio').checked = (json.AudioEdit === 'true');
		$('exportdir').selectedIndex = json.ExportDir;
		$('fnameprefix').value = json.FnamePrefix;
		$('longsuffix').checked = (json.long_suffix === 'true');
		$('editpath').value = json.EditPath;
//		$('dirsource').value = json.DirSource;					//database storage of director's cut has replaced file storage	
 	}
 	
 }


/*
 * creates an html link to an output file
 */
function fileLink(filename){
	var filenameText = addSoftHyphens(filename);
	var html = "<a href=" + "/cgi-bin/ingex-modules/Material.ingexmodule/download.pl?fileIn=" + filename + ">" + filenameText + "</a>";
	return html;
}


/*
 * add soft hyphens to a long piece of text (eg urls) so it can be split
 */
function addSoftHyphens(text){
	var next;
	var out = "";
	
	for(var i=0; i<text.length; i+=1){
		next = text.substring(i,i+1);
		out += "&shy;"+next;
	}	
	
	return out;
}
// 
// 
// /*
 // * an error occurred when loading tree data
 // */ 
// function treeLoadException(loader, node, response){
	// var title = "Communications Error!";
	// var message = 'Error when loading tree data';
	// show_static_messagebox(title, message);
	// insole.error(message);
// }
// 
// 
// /*
 // * search for a key word in clip names/descriptions
 // */
// function keywordSearch(){
	// submitFilter();	//apply filter
// }
// 
// 
/*
 * Calculate the number of materials on supplied path
 * Can contain both nodes which contain all the original materials and those which only contain some of them
 */
function getMaterialCount(n){
	var count = 0;
	var node = n;
	
	if(debug){alert('getmaterialcount');}
	// if(node instanceof Ext.tree.AsyncTreeNode){
		if(!node.get('leaf') && !node.isLoaded()){
			//there are unloaded children, so this path will contain all original materials
			
			//add this path to count
			return node.get('materialCount');
		}
	// }
	if(node.get('leaf')){
		//this is a material leaf
		
		//add 1 to count
		return 1;
	}
	else{
		//this is a loaded folder, which contians some children
		
		//investigate each child
		var children = node.childNodes;
		
		for(var i=0; i<children.length; i++){
			if(!children[i]){break;}
			count += getMaterialCount(children[i]);
		}
		
		return count;
	}
}

// 
/*
 * delete link was clicked
 */
function deleteClicked(dom){
	if(toDelete != ''){
		//still packages waiting to be deleted
		alert("Still processing a deletion request - wait a few seconds and retry");
		return;
	}
	if(dom == 'src'){
		delNodes = selectedSrcNodes;
		deleteMaterials(dom, delNodes);
	}
	else if(dom == 'dest'){
		delNodes = selectedDestNodes;
		deleteMaterials(dom, delNodes);
	}
}


// delete all
function deleteAllClicked(dom){
	delNodes = new Array();
			
	if(toDelete != ''){
		//still packages waiting to be deleted
		alert("Still processing a deletion request - wait a few seconds and retry");
		return;
	}	
	if(dom == 'src'){
		rootNode = tree.getRootNode();
		delNodes = rootNode.childNodes;
		
		count = getMaterialCount(delNodes[0]);
		
		// delNodes = rootNode.childNodes;
		deleteMaterials(dom, delNodes);
	}
	else if(dom == 'dest'){
		destrootNode = dest_tree.getRootNode();
		delNodes = destrootNode.childNodes;
		
		count = getMaterialCount(delNodes[0]);
		
		deleteMaterials(dom, delNodes);
	}
}


function deleteMaterials(dom, delNodes){
	if(dom == 'src'){
		if(delNodes.length > 0){
			var count = 0;
			
			for(var i=0; i<delNodes.length; i++)
			{
				if(!delNodes[i]){break;}
				
				count += getMaterialCount(delNodes[i]);
			}
			
			var response = confirm("About to permanently delete " + count + " materials packages! Are you sure?");
			
			if(response){
				//delete packages from database - will remove from tree when refreshed on callback
				toDelete = delNodes;
				delCall(delNodes, count);
			}
			else{
				//do nothing
			}
		}
	}
	else if(dom == 'dest'){
		if(delNodes.length > 0){
			var count = 0;
			
			for(var i=0; i<delNodes.length; i++)
			{
				if(!delNodes[i]){break;}
				
				count += getMaterialCount(delNodes[i]);
			}
			
			var response = confirm("Remove " + count + " materials from selection?");
			
			if(response){
				//yes
				
				//remove from tree
				removeNodesFromTree('dest', delNodes);
			}
		}
	}
}

// 
// //TODO - copy to external media
// function copyToExtMedia(dom){
	// alert('Not implemented yet');
// }
// 
// 
/*
 * removes node path from tree, updates material count and refreshes view
 */
function removeNodesFromTree(dom, nodes){
	if(dom == 'dest'){
		//update package size
		var count = 0;
		
		for(var i=0; i<nodes.length; i++)
		{
			if(!nodes[i]){break;}
			
			count += getMaterialCount(nodes[i]);
		}
		
		destPkgSize -= count;
		setDestPkgSize(destPkgSize);
	}
	
	for(var i=0; i<nodes.length; i++){
		//[higher level nodes will be removed first]
		if(!nodes[i]){break;}
		
		var next = nodes[i].get('name');
		if(debug){alert('removing ' + nodes[i].get('name'));}
		
		//set 'full' parameter of all parents to false, since this branch is no longer complete
		var parent = nodes[i];
		while(parent.getDepth() > 0){
			parent = parent.parentNode;
			parent.full = false;
		}
		
		if(nodes[i].getDepth() == 0){	//this is a root node
			if(debug){alert("root");}
				//can't delete node itself, so delete children
				while(nodes[i].firstChild){
					nodes[i].firstChild.remove();
				}
//				var children = nodes[i].childNodes;
//				for(var j in children){
//					if(debug){alert("next child " + children[j].text);}
//					nodes[i].removeChild(children[j]);
//				}
			}
			else{
				nodes[i].remove();
			}
		}
}
