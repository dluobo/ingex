/*
 * Copyright (C) 2011  British Broadcasting Corporation
 * Created 2008
 * Modified 2011
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
//approx 29.97 is NTSC frame rate
var ntscFps = (30000/1001);
//the number of recorders
var numOfRecs;
//the number of system time options
var numOfSysTimeOpts = 4;

//stores the list of series and programmes
var ILseriesData = new Object();

var tree = false;
var ILtreeLoader = false;
var ILrenderedExt;
var ILrootNode;
var ILcurrentItemId = null;
//a temporary node object
var ILtempNode = null;
//contains type of form to open
var ILformType;
//contains whether the expansion is after a new item has been added
var ILnewItemAdded = false;

var ILnoItemSelected = "No Item Selected";

//constants for 29.97 drop frame
var FRAMES_PER_DF_MIN = (60*30)-2; //1798 
//as every 10th minute does not drop two frames add 2
var FRAMES_PER_DF_TEN_MIN = (10 * FRAMES_PER_DF_MIN) + 2;//17982

var FRAMES_PER_DF_HOUR = (6 * FRAMES_PER_DF_TEN_MIN);//107892

//the number of milliseconds between each update from server.
var serverUpdatePeriod = 2000;

var onLoad_Logging = function() 
{
    checkJSLoaded(function(){
		Ext.onReady(function(){
			Logging_init();
		});
	});
};

//TODO find reason for why onLoad_Logging is called twice not once when reload page (browser reload button) whilst on logging tab
//if reload whilst on home and then navigate to logging tab it does not suffer this problem
loaderFunctions.Logging = onLoad_Logging;

//set ext spacer image to a local file instead of a remote url
//Ext.BLANK_IMAGE_URL = "../ingex/ext/resources/images/default/tree/s.gif";
//will be instance of timecode 'class'
var ILtc = false;

/// Initialise the interface, set up the tree, the editors etc, plus the KeyMap to enable keyboard shortcuts
function Logging_init () 
{
    ILrenderedExt = false;

//  check to see if tree is already rendered 	
    if(document.getElementById('takesList').innerHTML){ILrenderedExt = true;}

    ILtreeLoader = new Ext.tree.TreeLoader({
        dataUrl : "/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgramme.pl",
        uiProviders:{
            'take': Ext.tree.TakeNodeUI,
            'item': Ext.tree.ItemNodeUI
        }
    });

    ILtreeLoader.on("beforeload", function() 
    {
        if(document.getElementById('programmeSelector').selectedIndex != -1) 
        {
            ILtreeLoader.baseParams.progid = document.getElementById('programmeSelector').options[document.getElementById('programmeSelector').selectedIndex].value;
        } 
        else 
        {
            ILtreeLoader.baseParams.progid = -1;
        }
    }
    );

    ILtreeLoader.on("load", function()
    {
        if(ILnewItemAdded)
        {
            ILcurrentItemId = ILrootNode.lastChild.attributes.id;
           //DEBUG insole.warn("id = "+ILcurrentItemId);
            ILexpandSingleItem();
            ILnewItemAdded = false;
        }
        else
        {
            ILexpandSingleItem();
        }

    }
    );
	        
	//render if not currently rendered
	if(!ILrenderedExt)
	{ 
	    // root node	 
	    ILrootNode = new Ext.tree.AsyncTreeNode({
	        allowChildren: true,
	        draggable: false,
	        id: '0'	//level depth
	    });
	    
	    tree = new Ext.tree.ColumnTree({
	        width:'100%',
	        height: 240,
	        rootVisible:false,
	        autoScroll:true,
	        enableDD: false, /* required as of MultiSelectTree v 1.1 */
	        renderTo: 'takesList',
	        columns:[{
                header:'Dialogue No.',
                width:120,
                dataIndex:'sequence'
            },{
                header:'Name',
                width:130,
                dataIndex:'itemName'
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
	            width:70,
	            dataIndex:'result'
	        },{
	            header:'Comment',
	            width:220,
	            dataIndex:'comment'
	        }],

	        loader: ILtreeLoader,
	        root: ILrootNode
	    });
	   
	    //TODO add capability to sort the nodes and display them accordingly
	    /*
	var treeSorter = new Ext.tree.TreeSorter(tree, {
		//folderSort:true,
		//dir: "desc",
		//leafAttr: parseInt(takeNo),
		//sortType: ILgetFramesTotal

	});
	     */
	    
	    var treeEditor = new Ext.tree.ColumnTreeEditor(tree,{ 
            ignoreNoChange: true,
            editDelay: 0
        });
	    
	       
	    // render the tree
	    tree.render();

	    //Key mapping for the modules
	    	    
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
	            key: Ext.EventObject.G,
	            ctrl:true,
	            fn: ILsetCircled,
	            stopEvent: true
	        }, {
	            key: Ext.EventObject.B,
	            ctrl: true,
	            fn: ILsetNotCircled,
	            stopEvent: true
	        }, {
	            key: [Ext.EventObject.SPACE,Ext.EventObject.R],
	            ctrl: true,
	            fn: ILstartStop,
	            stopEvent: true
	        },{
	            key: Ext.EventObject.C,
	            ctrl: true,
	            shift: true,
	            fn: function() {document.getElementById('commentBox').focus();},
	            stopEvent: true
	        }, {
	            key: Ext.EventObject.DELETE,
	            ctrl:true,
	            fn: ILdeleteItem,
	            stopEvent: true
	        }, {
	            key: Ext.EventObject.N,
	            ctrl: true,
	            fn: function() {ILformCall('new');},
	            stopEvent: true
	        }, {
	            key: [Ext.EventObject.P,Ext.EventObject.D],
	            ctrl: true,
	            fn: function() {ILformCall('pickup');},
	            stopEvent: true
	        },{
	            key: Ext.EventObject.UP,
	            stopEvent: true,
	            ctrl: true,
	            fn: ILselectFirstItem
	            
	        }, {
	            key: Ext.EventObject.DOWN,
	            stopEvent: true,
	            ctrl: true,
	            fn: ILselectLastItem
	            
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
	        }
	         
	    ]);
	
	}//end if not rendered yet  !ILrenderedExt
	
	//gets the available recorders
	ILdiscoverRecorders();
	//gets the recorder locations, series and the programmes within the series
	ILpopulateSeriesLocInfo();
        //create new ingexLiggingTimecode object
        ILtc = new ingexLoggingTimecode();
        document.getElementById('stopButton').disabled = true;
}// end function logging_init()

//determines if a series is selected
function ILcheckSeriesSelected()
{
    var seriesSel = document.getElementById('seriesSelector');
    if (seriesSel.value == 0 || seriesSel.value == -1)//no series selected
    {   
        Ext.MessageBox.alert('ERROR!', 'NO SERIES SELECTED : Please select a series');
        seriesSel.style.color = "rgb(255,0,0)";
        return false;
    }
    else //series is selected
    {
        return true;
    }

}//end is series selected

//determines if a programme is selected
function ILcheckProgSelected()
{
    var progSel = document.getElementById('programmeSelector'); 
    if(progSel.value == -1) //no programme selected
    {
        Ext.MessageBox.alert('ERROR!', 'NO PROGRAMME SELECTED : Please select a programme');
        progSel.style.color = "rgb(255,0,0)";
        return false;
        
    }
    else //programme is selected
    {
        return true;
    }
}//end is programme selected

//determines if a timecode source is selected
function ILcheckRecorderSelected()
{
    var recSel = document.getElementById('recorderSelector');
    if(recSel.options[recSel.selectedIndex].value == -1)
    {
        Ext.MessageBox.alert('ERROR!', 'NO RECORDER SELECTED : Please select a recorder');
        recSel.style.color = "rgb(255,0,0)";
        return false;
    }
    else //recorder is selected
    {
        return true;
    }
}//end is recorder selected

function ILexpandItems()
{
    tree.expandAll();
}

function ILcollapseItems()
{
    tree.collapseAll();
}

//expands the currently selected item
function ILexpandSingleItem()
{
    var seriesSel = document.getElementById('seriesSelector');
    if (seriesSel.value != 0 && seriesSel.value != -1)//series selected
    {   
        var progSel = document.getElementById('programmeSelector'); 
        if(progSel.value != -1) //programme selected
        {
            var progNode = tree.getRootNode();
           
            if(progNode.hasChildNodes())//if there are items
            {
               //collapse all items
               ILcollapseItems();
               if (ILcurrentItemId != null)
               {                   
                   
                       ILtempNode = tree.getNodeById(ILcurrentItemId);
                       tree.getSelectionModel().select(ILtempNode);
                       ILtempNode.expand();
                       document.getElementById('currentItemName').innerHTML = ILtempNode.attributes.itemName;
                       document.getElementById('currentItemName').className = 'itemSelected';
                       document.getElementById('currentTakeNum').className = 'itemSelected';
                       //if this item is not empty
                       
                       if(ILtempNode.lastChild != null)
                       {
                   
                           
                           var takeNumber;
                           //find the takeNumber of the last child (take) and add one
                           takeNumber = Number(ILtempNode.lastChild.attributes.takeNo) + 1;
                           //DEBUG insole.warn("finding the last child take number = "+ILtempNode.lastChild.attributes.takeNo);
                           document.getElementById('currentTakeNum').innerHTML = takeNumber;
                           document.getElementById('currentTakeNum').className = 'itemSelected';
                       }//end item has takes
                       else
                       {
                         ILresetTakeNum();
                       }
                }//end is current item != null
            }//if programme has items
        }//if a programme is selected   
    }//if a series selected
}//end expandSingleItem()

//select first item in tree
function ILselectFirstItem() 
{
    if (tree.getRootNode() != null && tree.getRootNode().firstChild != null)
    {
        tree.getSelectionModel().select(tree.getRootNode().firstChild);
        ILtempNode = tree.getRootNode().firstChild;
        ILcurrentItemId = ILtempNode.id;
        ILexpandSingleItem();
    }

 }//end select first item

//select last item in tree
function ILselectLastItem()
{
    if (tree.getRootNode() != null && tree.getRootNode().lastChild != null)
    {
        tree.getSelectionModel().select(tree.getRootNode().lastChild);
        ILtempNode = tree.getRootNode().lastChild;
        ILcurrentItemId = ILtempNode.id;
        ILexpandSingleItem();
    }
}//end select last item

/// Select the previous item in the item list
function ILselectNextItem() 
{
    if(ILcurrentItemId != null)
    {
        ILtempNode = tree.getNodeById(ILcurrentItemId);
        var ILtempNodeDepth = ILtempNode.getDepth();
        if(ILtempNode.getDepth() == 1)
        {
            if (ILtempNode.nextSibling != null)
            {    
                ILtempNode = ILtempNode.nextSibling;
            }//end if has a next sibling
        }//end if is an item
        else
        {
            if(ILtempNodeDepth == 2)//is a take
            {
                if (ILtempNode.parentNode.nextSibling != null)
                {    
            
                ILtempNode = ILtempNode.parentNode.nextSibling;  
                }//end has next Sibling
             }//end is a take
            //add code here if event marker etc.
        }//end is a take or deeper
            ILcurrentItemId = ILtempNode.id;
            ILexpandSingleItem();
      }//end if something is selected
}//end select next item

/// Select the next item in the item list
function ILselectPrevItem() 
{
    if(ILcurrentItemId != null)
    {
        ILtempNode = tree.getNodeById(ILcurrentItemId);
        var ILtempNodeDepth = ILtempNode.getDepth();
        if(ILtempNode.getDepth() == 1)
        {
            if (ILtempNode.previousSibling != null)
            {
                ILtempNode = ILtempNode.previousSibling;
            }//end if has a previous sibling
            
        }//end if is an item
        else
        {
            if(ILtempNodeDepth == 2)//is a take
            {
                if (ILtempNode.previousSibling != null)
                {
                    ILtempNode = ILtempNode.parentNode.previousSibling;  
                }
            }
            //add code here if event marker etc.
        }//end is a take or deeper
        ILcurrentItemId = ILtempNode.id;
        ILexpandSingleItem();
    }//end if something is selected
}//end select previous item


function ILresetTakeNum()
{
    document.getElementById('currentTakeNum').innerHTML = "1";
    document.getElementById('currentItemName').className = 'itemSelected';
}//end resetTakeNum()


//resets the displayed item name
function ILresetItemName()
{
  //DEBUG insole.warn("resetting item");
    ILtempNode = tree.getSelectionModel().getSelectedNode();
    if (ILtempNode != null) 
    {
        ILtempNode.unselect();
    }
    ILcurrentItemId = null;
    ILtempNode = null;
    document.getElementById('currentItemName').innerHTML = ILnoItemSelected;
    document.getElementById('currentItemName').className = 'itemNameNone';
    ILresetTakeNum();
}//end resetItemName()

function ILreplaceSlashQuotesNewLines(inputString)
{
    //need to make sure that itemname does not include \ or " in unescaped manner and no ' as breaks either html or json object
    var nameRegExpBacklash = /\\/g;
    var nameRegExpDoubQuote = /\"/g;
    var nameRegExpSingQuote = /'/g;
    var nameRegExpNewLine = /\n/g;
    //backslash has to be escaped first otherwise breaks other escaping
    inputString = inputString.replace(nameRegExpBacklash, "\\\\");
    inputString = inputString.replace(nameRegExpNewLine, "\\n");
    inputString = inputString.replace(nameRegExpDoubQuote, "\\\"");
    inputString = inputString.replace(nameRegExpSingQuote, "\\\"");
    
    return inputString;
}//end ILreplaceSlashAndQuotes

function ILresultToID(resultString)
{
    // TODO make this result string come from database on load of page
    var resultID;
    if(resultString == "Circled")
    {
        resultID = 2;
    }
    else if (resultString == "Not Circled")
    {
        resultID = 3;
    }
    else
    {
        resultID = 1;
    }
    return resultID;
}

/// Convert a textual timecode string (hh:mm:ss:ff) to a number of frames
/// @param text the timecode string, boolean whether using dropframe
/// @return the number of frames as integer
function ILgetFramesTotalFromText (text, frameDrop) 
{
  //DEBUG insole.warn("text to convert ="+text+" and dropframe status="+frameDrop);
  //Using dropframe timecode
  if(frameDrop)
  {
      //DEBUG insole.warn("splitting by semicolon framestotalfrom text");
      //separate string into constituent values using semi-colon
      var chunks = text.split(";");
  }
  else //NOT using dropframe timeocde
  {
      //split text into values based on colon separator
      var chunks = text.split(":");
  }
  //convert text into integers base 10 (decimal)
  var h = parseInt(chunks[0], 10);
  var m = parseInt(chunks[1], 10);
  var s = parseInt(chunks[2], 10);
  var f = parseInt(chunks[3], 10);

  //if using dropframe timecode
  if(frameDrop)
  {
      //DEBUG insole.warn("total from text "+h+":"+m+":"+s+":"+f+"  witheditrate= "+editrate);
      //nominal fps = 30 for 29.97 so round to 30 for calculation
      var totFrames = (f  
                      + (s * Math.round(editrate) ) 
                      + ((m % 10) * FRAMES_PER_DF_MIN) 
                      + Math.floor(((m / 10) * FRAMES_PER_DF_TEN_MIN)) 
                      + (h * FRAMES_PER_DF_HOUR) );
      
      //DEBUG insole.warn("drop frame framecount returned="+totFrames);
      return  (f 
              + (s * Math.round(editrate)) 
              + ((m % 10) * FRAMES_PER_DF_MIN) 
              + Math.floor( ( (m/10) * FRAMES_PER_DF_TEN_MIN))
              + (h * FRAMES_PER_DF_HOUR) );
          
  }
  else //NOT using dropframe
  {
      //for 29.97 have to use base 30 for timecode so round
      if (editrate == ntscFps)
      {
          //DEBUG insole.warn("total from text"+h+":"+m+":"+s+":"+f+"  witheditrate="+editrate);
          var totFrames = (Math.floor(h*60*60* Math.round(editrate) ) + Math.floor(m*60* Math.round(editrate) ) + Math.floor(s* Math.round(editrate)) + f);
          //DEBUG insole.warn("framecount returned="+totFrames);
          return (Math.floor(h*60*60* Math.round(editrate) ) + Math.floor(m*60* Math.round(editrate) ) + Math.floor(s* Math.round(editrate)) + f);
      }
      else //using non-dropframe integer framerates
      {
          //DEBUG insole.warn("total from text"+h+":"+m+":"+s+":"+f+"  witheditrate="+editrate);
          var totFrames = (Math.floor(h*60*60*editrate) + Math.floor(m*60*editrate) + Math.floor(s*editrate) + f);
          //DEBUG insole.warn("framecount returned="+totFrames);
          return (Math.floor(h*60*60*editrate) + Math.floor(m*60*editrate) + Math.floor(s*editrate) + f);
      }
  }//end NOT using dropframe
}//end ILgetFramesTotalFromText()


/// Convert a number of frames to a timecode string
/// @param length the number of frames, frame rate
/// @return the timecode string
function ILgetTextFromFramesTotal (clipLength, timEditRate) 
{
  var h; var m; var s; var f;
  //insole.alert("get text from frames total needs fixing");
  
  //DEBUG insole.warn("framecount ="+clipLength+"edit rate="+timEditRate);
  
  //find number of whole seconds within clip length
//i.e. throw away the remainder - that is covered by the frames
  s = Math.floor(clipLength/timEditRate);
  //number of whole minutes within clip length
  m = Math.floor(s/60);
  //number of whole hours within the clip length
  h = Math.floor(m/60);
  
  //DEBUG insole.warn("frameCount="+frameCount+" minus this"+(editrate * this.s));
  //length - number of whole seconds gives number of frames
  f = clipLength - (timEditRate * s);
  //number of whole seconds within the clip length - (60*number of whole min within clip length)
  s = s - (60 * m);
  //number of whole minutes within the clip length - (60*number of whole hours within clip length)
  m = m - (60 * h);
  
  //DEBUG insole.warn("this.f ="+this.f+"  before rounding down");
  //round down number of frames for case where non-drop non-integer frame rate timecode
  f = Math.floor(f);
          
  if (editrate == ntscFps) //if edit rate = 29.97
  {
      //Using Dropframe
      if(dropFrame)
      {
          //DEBUG insole.warn("within update from count h= "+this.h+" m="+this.m+"  s="+this.s+"  f="+this.f);
          
          //if using system time or just incrementing then need to work out the timecode from framecount
          var countOfFrames = clipLength;
          h = Math.floor( clipLength / FRAMES_PER_DF_HOUR);
          countOfFrames = countOfFrames % FRAMES_PER_DF_HOUR;
          var ten_min = Math.floor(countOfFrames / FRAMES_PER_DF_TEN_MIN);
          countOfFrames = (countOfFrames % FRAMES_PER_DF_TEN_MIN);
          
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
      }//end is using dropframe timecode
      
      else //NOT using dropframe
      {
      //will have already been covered by 
      }
  }//if is edit rate = 29.97
  
  // ensure that hours never goes above 23 and wraps round
  if (h > 23)
  {
      h = h % 24;
  }
  
  //ensure that prefix zero for single value times e.g 9 becomes 09
  if (h < 10) { h = "0" + h; }
  if (m < 10) { m = "0" + m; }
  if (s < 10) { s = "0" + s; }
  if (f < 10) { f = "0" + f; }
  
  if(dropFrame)
  {
      return h+";"+m+";"+s+";"+f;
  }
  else
  {
      return h+":"+m+":"+s+":"+f;
  }
}//end ILgetTextFromFramesTotal() 

/// Delete the currently selected item
function ILdeleteItem () 
{
    var seriesSelected = ILcheckSeriesSelected();
    if (seriesSelected)
    {
        var progSelected = ILcheckProgSelected();
        if (progSelected)
        {
            if(tree && tree.getSelectionModel().getSelectedNode()) 
            {
                var ILtempNode = null;
                //if an item is selected
                if(tree.getSelectionModel().getSelectedNode().attributes.itemName) {
                    ILtempNode = tree.getSelectionModel().getSelectedNode();
                    var conf = confirm('Are you sure you wish to delete the item : [  '
                            +ILtempNode.attributes.itemName+' ] and all its takes? You cannot undo this.');
                } 
                // if a take is selected 
                else if (tree.getSelectionModel().getSelectedNode().parentNode.attributes.itemName){
                    ILtempNode = tree.getSelectionModel().getSelectedNode().parentNode;
                    var conf = confirm('Are you sure you wish to delete the item: '+ILtempNode.attributes.itemName+' and all its takes? You cannot undo this.');
                }
                //if confirmed that wish to delete
                if (conf)
                {
                    // -- Remove from database
                    Ext.Ajax.request({
                        url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/deleteItem.pl',
                        params : {
                            id: ILtempNode.attributes.databaseId
                        },
                        success: function(response) {
                            try {
                                var data = JSON.parse(response.responseText);
                            } catch (e) {
                                insole.log("JSON parse error: "+e.name+" - "+e.message);
                                insole.log("JSON data was: "+response.responseText);
                                insole.alert("Failed to delete item from database: "+ILtempNode.attributes.itemName+". See previous log message.");
                            }
                            if(data.success) {
                                //reload tree view
                                ILloadNewProgInfo();

                                insole.log("Successfully deleted item from database: "+ILtempNode.attributes.itemName);
                            } else {
                                insole.error("Error adding to database: "+data.error);
                                insole.alert("Failed to deleting item from database: "+ILtempNode.attributes.itemName+". See previous log message.");
                            }
                        },
                        failure: function() {
                            insole.alert("Failed to delete item from database: "+ILtempNode.attributes.itemName+". Recommend you refresh this page to check data integrity.");
                        }
                    });
                }//end if confirmed that wish to delete
            }//if tree exists and node selected
            else
            {
                Ext.MessageBox.alert('ERROR Deleting Item!', 
                'Unable to delete item - NO ITEM SELECTED : Please select an item before deleting');
            }
        }//end programme selected
    }//end series selected
}//end ILdeleteItem


//validates item input ensuring not too many characters or empty
function ILvalidateItemInput(newItemName, seqStart, seqEnd)
{
  //reset the error messages and keep current input
  if (document.getElementById('itemInputError').innerHTML != "")
  {
      document.getElementById('itemInputError').innerHTML = "";
      document.getElementById('itemNameInputHeading').className = "itemPopupHeading";
      document.getElementById('seqInputHeading').className = "itemPopupHeading";
  }//end if an input error was previously present
  
  var seq = seqStart+","+seqEnd;
  
  if (newItemName.length < 512)
  {
      //DEBUG insole.warn("seq is ="+seq);
      if (seq.length < 510)//valid length of input
      { 
          //no point checking an empty string
          var ILstartEmpty= true;
          var ILendEmpty= true;
          //check that only contains numeric characters in to and from sequence numbers
          var ILnotOnlyDigits = /\D/g;
          if (seqStart.length != 0)
          {
              ILstartEmpty=false;
              if (ILnotOnlyDigits.test(seqStart))
              {
                  //throw error and inform
                  document.getElementById('seqInputHeading').className = "itemPopupHeadingError";
                  document.getElementById('itemInputError').innerHTML = "<span>ERROR In Dialogue Number - FROM - Only Numbers are allowed</span>";
                  insole.warn("Start contains non numeric characters");
                  return false;
              }
          }
          if (seqEnd.length != 0)
          {
              ILendEmpty=false;
              if (ILnotOnlyDigits.test(seqEnd))
              {
                  //throw error and inform
                  document.getElementById('seqInputHeading').className = "itemPopupHeadingError";
                  document.getElementById('itemInputError').innerHTML = "<span>ERROR In Dialogue Number - TO - Only Numbers are allowed</span>";
                  insole.warn("End contains non numeric characters");
                  return false;
              }
          }
          if(!ILstartEmpty && !ILendEmpty)
          {
              //if both are not empty check that end is after start
              //parse as integer and compare
              var seqStartInt = parseInt(seqStart, 10);
              var seqEndInt = parseInt(seqEnd, 10);
              if (seqEndInt < seqStartInt)
              {
                  document.getElementById('seqInputHeading').className = "itemPopupHeadingError";
                  document.getElementById('itemInputError').innerHTML = "<span>ERROR In Dialogue Number - End before the start</span>";
                  insole.warn("End is before the start");
                  return false;
              }

              if (seqStartInt == 0 && seqEndInt == 0)
              {
                  document.getElementById('seqInputHeading').className = "itemPopupHeadingError";
                  document.getElementById('itemInputError').innerHTML = "<span>ERROR In Dialogue Number - Cannot have both start and end as zero </span>";
                  insole.warn("Both start and end zero - Not allowed");
                  return false;
              }
          }//end both start and end not empty
  
          if (ILstartEmpty && ILendEmpty) //both are empty 
          {
              document.getElementById('seqInputHeading').className = "itemPopupHeadingError";
              document.getElementById('itemInputError').innerHTML = "<span>ERROR In Dialogue Number - Cannot have both start and end empty </span>";
              insole.warn("Both start and end empty - Not allowed");
              return false;
          }
          return true;
      }
      else
      {
          //max length of seq must be 2 shorter to allow for {} bounding
          var diffChar = seq.length - 510;
          document.getElementById('seqInputHeading').className = "itemPopupHeadingError";
          document.getElementById('itemInputError').innerHTML = "<span>ERROR In Dialogue Number - Too Many Characters Used : Please reduce by "+diffChar+" Characters</span>";
          return false;
      }
  }
  else
  {
      var diffChar = newItemName.length - 512;
      document.getElementById('itemNameInputHeading').className = "itemPopupHeadingError";
      document.getElementById('itemInputError').innerHTML = "<span>ERROR In Item Name - Too Many Characters Used : Please reduce by "+diffChar+" Characters</span>";
      return false;
  }
}//end ILvalidateInput()

//stores an item within the database
function ILstoreItem (itemName, itemSeq)
{
  var progSel = document.getElementById('programmeSelector');
  var newID;
  //get the children of the root node (the items for this programme)
  var children = tree.getRootNode().childNodes; //get the items
  //if there are items get the last one
  if (ILrootNode.lastChild != null){
      var lastItem = ILrootNode.lastChild;
      newID = Number(lastItem.attributes.orderIndex) + 1;
  }
  else //if no items already exist, set to first item in order index 
  {
      newID = 0;
  }
  //add correct braces for storage
  itemSeq ="{"+itemSeq+"}";
  insole.log("adding item "+itemName+"  seq="+itemSeq+" progid="+progSel.value+" new item id="+newID);
  
  itemName = ILreplaceSlashQuotesNewLines(itemName);
  var jsonText = "{\"ITEMNAME\":\""+itemName+"\", \"SEQUENCE\":\""+itemSeq+"\", \"PROGRAMME\":\""+progSel.value+"\", \"ORDERINDEX\":\""+newID+"\"}";
  // -- Send to database
  Ext.Ajax.request({
      url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/addItem.pl',
      params : {
       jsonIn: jsonText
  },
  success: function(response) {
      try {
          var data = JSON.parse(response.responseText);
      } catch (e) {
          insole.log("JSON parse error: "+e.name+" - "+e.message);
          insole.log("JSON data was: "+response.responseText);
          insole.alert("Failed to add item to database: "+itemName+". See previous log message.");
      }
      if(data.success) {
        //tell tree on load function that is after a new item addition so selects new item
          ILnewItemAdded = true;
          //reload the tree
          ILrefreshTree();
          insole.log("Successfully added item to database: "+itemName);
      } else {
          insole.error("Error adding to database: "+data.error);
          insole.alert("Failed to add item to database: "+itemName+". See previous log message.");
          ILrefreshTree();
      }
  },
  failure: function() {
      insole.alert("Failed to add item to database: "+itemName);
      ILrefreshTree();
  }
  });
 
}//end function ILstoreItem


//send the information to the database for the updated item
function ILupdateItemInDb(newItemName, newSeq, dbId)
{
  insole.warn("sending for update "+newItemName+" "+newSeq+" "+dbId);

  //ensure sequence is bounded by braces {} to create list
  newSeq = "{"+newSeq+"}";
  insole.log("adding item "+newItemName+"  seq="+newSeq+"database id="+dbId);

  newItemName = ILreplaceSlashQuotesNewLines(newItemName);
  
  var jsonText = "{\"ITEMNAME\":\""+newItemName+"\", \"SEQUENCE\":\""+newSeq+"\"}";
  
  // -- Send to database
  Ext.Ajax.request({
      url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/changeItem.pl',
      params : {
          id: dbId,
          jsonIn: jsonText
      },
      success: function(response) {
          try {
              var data = JSON.parse(response.responseText);
          } catch (e) {
              insole.log("JSON parse error: "+e.name+" - "+e.message);
              insole.log("JSON data was: "+response.responseText);
              insole.alert("Failed to update database for item with id: "+dbId+"See previous log message.");
          }
          if(data.success) {
              insole.log("Successfully updated database for item with id : "+dbId);
              ILrefreshTree();
          }
          else{
              insole.error("Error updating database: "+data.error);
              insole.alert("Failed to update database for item with id :"+dbId+" See previous log message.");
          }
      },
      failure: function() {
         
          insole.alert("Failed to update database for item with id: "+dbId);
      }
  }); 
}//end ILupdateIteminDB()

//process item update form and determine if a change has taken place
/*function ILitemUpdate (ILnewItemName, ILnewSeq)
{
    //process changes, update if necessary and then close the window
    ILtempNode = tree.getNodeById(ILcurrentItemId);
    var dbId = ILtempNode.attributes.databaseId;
    var ILoldItemName = ILtempNode.attributes.itemName;
    var ILoldSeq = ILtempNode.attributes.sequence;
  
    //if there are any changes
    if(ILnewItemName != ILoldItemName || ILnewSeq != ILoldSeq)
    {
        //update item name and seq
        ILupdateItemInDb(ILnewItemName, ILnewSeq, dbId);
        //DEBUG 
        insole.warn("sending "+ILnewItemName+" "+ILnewSeq+" "+dbId);
    }   
}*///end ILitemUpdate()

/// Start/stop timecode updates for a take
function ILstartStop ()
{
  
    // TODO fix this so on key up space bar doesn't auto enter on the ok button in ext messagebox
    // temp solution is to use some key other than space
    // could alter creation of message box to be self created and ensure focus is not on ok button so space doesn't 
    // or could change away from 'pop-up' alert style to a notification area
    var seriesSelected = ILcheckSeriesSelected();
    
    if (seriesSelected)
    {
        var progSelected = ILcheckProgSelected();

        if (progSelected)
        { 
            var ILrecSelected = ILcheckRecorderSelected();
            
            if (ILrecSelected)
            {
                if(tree && tree.getSelectionModel().getSelectedNode()) 
                {
                    //if no take is running
                    if(ILtc.takeRunning == false)
                    {
                        // START and change text of button to Stop
                        // document.getElementById('startStopButton').innerHTML = 'Stop';
                        //document.getElementById('startStopButton').className = 'stopButton';
                        //start the take
                        ILtc.startTake();
                        /*move focus to comment box (this also ensures if focus was on any element that will be disabled that
                            it won't break key mapping by operating on disabled item)*/
                        document.getElementById('commentBox').focus();
                        //Change the background colour of the section to indicate in record mode
                        // change to be a class change rather than hard coded colour change
                        document.getElementById('newTake').style.backgroundColor = '#FFEEEE' ;
                        //disable start and enable stop button
                        document.getElementById('startButton').disabled = true;
                        document.getElementById('stopButton').disabled = false;
                        //disable series, programme, location, tree reset, delete Item,new Item, Pickup and Import
                        document.getElementById('seriesSelector').disabled = true;
                        document.getElementById('programmeSelector').disabled = true;
                        document.getElementById('recLocSelector').disabled = true;
                        document.getElementById('resetButton').disabled = true;
                        document.getElementById('newItemButton').disabled = true;
                        document.getElementById('deleteItemButton').disabled = true;
                        document.getElementById('pickupItemButton').disabled = true;
                        document.getElementById('importButton').disabled = true;
                        //disable the recorder selector whilst a take is running
                        document.getElementById('recorderSelector').disabled = true;
                    }//end there is no take running
                    else //there is a take already running
                    {
                        // STOP and change button text to stop
                        //document.getElementById('startStopButton').innerHTML = 'Start';
                        // document.getElementById('startStopButton').className = 'startButton';
                        //stop take
                        ILtc.stopTake();
                        //enable start and disable stop button
                        document.getElementById('startButton').disabled = false;
                        document.getElementById('stopButton').disabled = true;
                        //enable series, programme, location, tree reset, delete Item,new Item, Pickup and Import
                        document.getElementById('seriesSelector').disabled = false;
                        document.getElementById('programmeSelector').disabled = false;
                        document.getElementById('recLocSelector').disabled = false;
                        document.getElementById('resetButton').disabled = false;
                        document.getElementById('newItemButton').disabled = false;
                        document.getElementById('deleteItemButton').disabled = false;
                        document.getElementById('pickupItemButton').disabled = false;
                        document.getElementById('importButton').disabled = false;
                        //enable the recorder selector whilst a take is NOT running
                        document.getElementById('recorderSelector').disabled = false;
                        //indicate not logging take
                        document.getElementById('newTake').style.backgroundColor = '#FFFFEE';
            
                    }//end there is a take already running
                }//end if item selected
                else
                {
                    Ext.MessageBox.alert('ERROR!', 'NO ITEM SELECTED : Please select an item');
                }
            }//end a recorder selected
        } //end a programme is selected
    }//end a series is selected
}//end ILstartStop()

//send the information to the database for the updated node
function ILupdateTakeInDb(newResult, newComment, dbId)
{

newResult = ILresultToID(newResult);
newComment = ILreplaceSlashQuotesNewLines(newComment);

//process changes, update if necessary and then close the window
ILtempNode = tree.getNodeById(ILcurrentItemId);
var ILoldComment = ILtempNode.attributes.comment;
var ILoldResult = ILtempNode.attributes.sequence;



var jsonText ="{\"RESULT\":\""+newResult+"\",\"COMMENT\":\""+newComment+"\"}"; 
  
// -- Send to database
Ext.Ajax.request({
    url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/changeTake.pl',
    params : {
        result: newResult,
        comment: newComment,
        id: dbId,
        jsonIn: jsonText
    },
    success: function(response) {
        try {
            var data = JSON.parse(response.responseText);
        } catch (e) {
            insole.log("JSON parse error: "+e.name+" - "+e.message);
            insole.log("JSON data was: "+response.responseText);
            insole.alert("Failed to update database for take with id: "+dbId+"See previous log message.");
        }
        if(data.success) {
            insole.log("Successfully updated database for take with id : "+dbId);
            ILrefreshTree();
        }
        else{
            insole.error("Error updating database: "+data.error);
            insole.alert("Failed to update database for take with id :"+dbId+" See previous log message.");
        }
    },
    failure: function() {
       
        insole.alert("Failed to update database for take with id: "+dbId);
    }
}); 
}//end ILupdateTakeinDB()

/// Store the new take to database and refresh display (reload tree)
function ILstoreTake () 
{
    var ILseriesSel = ILcheckSeriesSelected();
    var ILprogSel = ILcheckProgSelected();
    if(ILseriesSel)
    {
		if(ILprogSel) //a programme is selected
		{
		    //get the duration value
		    var duration = document.getElementById('durationBox').value;
		    //if take has no duration
		    if(duration == "00:00:00:00")
		    {
		        Ext.MessageBox.alert('ERROR adding take!', 'Unable to add take - ZERO LENGTH DURATION : Please make a log before adding a take');
		    }
		    else //take has a real duration
		    {
		        //get location and item
		        var locSel = document.getElementById("recLocSelector");
		        if (ILtc.takeRunning)//take is running
		        {
		            Ext.MessageBox.alert('ERROR adding take!', 'Unable to add take when TAKE IS RUNNING: Please stop take before adding a take');
		        }
		        else //take is not running
		        {
		            //reload item so is updated to current status
		            ILtempNode = tree.getNodeById(ILcurrentItemId);
		            //if there is a current node selected
		            if (ILtempNode != null)
		            {
		                
		                var ILnodeDepth =ILtempNode.getDepth(); 
		                if (ILnodeDepth > 1) //is a take or lower
		                {
                            //if is an event marker or take child then need to add code here to separate cases
		                    //select the parent item of a take
		                    ILtempNode = ILtempNode.parentNode;
		                    ILcurrentItemId = ILtempNode.id;
		                }
		                var now = new Date();
		                var dd = now.getDate();
		                var mm = now.getMonth() + 1; // it ranges 0-11 so add 1
		                var yy = now.getFullYear();
		                if (dd < 10) { dd = "0" + dd; }
		                if (mm < 10) { mm = "0" + mm; }
		                var sqlDate = yy+"-"+mm+"-"+dd;

		                var locationName = locSel.options[locSel.selectedIndex].innerHTML;
		                var locationID = locSel.options[locSel.selectedIndex].value;

		                var result = document.getElementById('resultText').innerHTML;

		                var comment = document.getElementById('commentBox').value;
		                if (comment == "") comment  = "[no data]";
		                if (comment.length < 512)
		                {
		                    var inpoint = document.getElementById('inpointBox').innerHTML;
		                    var outpoint = document.getElementById('outpointBox').innerHTML;
		                    //DEBUG insole.warn("dropframe status = "+dropFrame);
		                    if (dropFrame)
		                    {
		                        var start = ILgetFramesTotalFromText(inpoint, true);
		                        //DEBUG insole.warn("dropframe total frames returned = "+start);
		                    }
		                    else 
		                    {
		                        var start = ILgetFramesTotalFromText(inpoint, false);
		                    }
		                    var clipLength = ILgetFramesTotalFromText(duration, false);
		                    var editRateCombined = "("+frameNumer+","+frameDenom+")";
		                    //DEBUG insole.warn("edit rate:"+editRateCombined);
		                    //convert from current option item to the database id
		                    var itemIdent = ILtempNode.attributes.databaseId;
		                    var takeNumber;
		                    //if this item is not empty
		                    if(ILtempNode.lastChild != null)
		                    {
		                        //find the takeNumber of the last child (take) and add one
		                        takeNumber = Number(ILtempNode.lastChild.attributes.takeNo) + 1;
		                        //DEBUG insole.warn("finding the last child take number = "+ILtempNode.lastChild.attributes.takeNo);
		                    } 
		                    else //item is empty
		                    {
		                        takeNumber = 1;
		                    }
		                    
		                    comment = ILreplaceSlashQuotesNewLines(comment);
		                    result = ILresultToID(result);
		                    
		                    var jsonText = "{\"TAKENO\":\""+takeNumber+"\",\"LOCATION\":\""+locationID+
		                    "\",\"DATE\":\""+sqlDate+"\",\"START\":\""+start+
		                    "\",\"LENGTH\":\""+clipLength+"\",\"RESULT\":\""+result+
		                    "\",\"COMMENT\":\""+comment+"\",\"ITEM\":\""+itemIdent+
		                    "\",\"EDITRATE\":\""+editRateCombined+"\"}";
		                    
		                    // -- Send to database
		                    Ext.Ajax.request({
		                        url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/addTake.pl',
		                        params : {
		                        jsonIn: jsonText
		                    },
		                    success: function(response) {
		                        try {
		                            var data = JSON.parse(response.responseText);

		                        } catch (e) {
		                            insole.log("JSON parse error: "+e.name+" - "+e.message);
		                            insole.log("JSON data was: "+response.responseText);
		                            insole.alert("Failed to add take to database. Item: "+ILtempNode.attributes.itemName+", Take Number: "+takeNumber+". See previous log message.");
		                        }
		                        if(data.success) {
		                            insole.log("Successfully added take to database. Item: "+ILtempNode.attributes.itemName+", Take Number: "+takeNumber);
		                            //reset the values to be stored in a take to default
		                            ILresetTake();

		                            //refresh the tree display from the database
		                            //automatically clears previous nodes on load by default
		                            //url from which to get a JSON response
		                            ILtreeLoader.url = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgramme.pl';
		                            //load the info to a root node
		                            ILtreeLoader.load(tree.getRootNode());

		                        } else {
		                            insole.error("Error adding to database: "+data.error);
		                            insole.alert("Failed to add take to database. Item: "+ILtempNode.attributes.itemName+", Take Number: "+takeNumber+". See previous log message.");
		                        }
		                    },
		                    failure: function() {
		                        insole.alert("Failed to add take to database. Item: "+ILtempNode.attributes.itemName+", Take Number: "+takeNumber);
		                    }
		                    });

		                }//if comment is of acceptable length
		                else
		                {
		                    //messagebox saying too many characters
		                    var diffChar = comment.length - 512;
		                    Ext.MessageBox.alert('ERROR!', 'Too Many Characters ('+comment.length +') Used : Please reduce by '+diffChar+' Characters');

		                }
		            }//end an item is selected
		            else 
                    {
                        Ext.MessageBox.alert('ERROR adding take!', 'Unable to add take when NO ITEM SELECTED: Please select an item first');
                    }
		        }//end take is not running
		    }//end take has a non-zero duration
		}//end a programme is selected
	}//end a series is selected
}//end ILstoreTake


/// Reset new take details to defaults
function ILresetTake () {
	
    if (ILtc.takeRunning)//take is running
    {
        Ext.MessageBox.alert('ERROR resetting take details!', 'Unable to reset take details when TAKE IS RUNNING: Please stop take before resetting');

    }
    else
    {
        document.getElementById('inpointBox').innerHTML = "00:00:00:00";
        document.getElementById('outpointBox').innerHTML = "00:00:00:00";
        document.getElementById('durationBox').innerHTML = "00:00:00:00";
        document.getElementById('commentBox').value = "";
        document.getElementById('resultText').innerHTML = "Not Circled";
        document.getElementById('resultText').className = "notCircled";
    }
}//end ILresetTake()

/// Set the new take's result to "Circled"
function ILsetCircled() {
    
    if(tree && tree.getSelectionModel().getSelectedNode()) 
    {
        //check that an item/take is selected
	document.getElementById('resultText').innerHTML = "Circled";
	document.getElementById('resultText').className = "circled";
    }
    else
    {
         Ext.MessageBox.alert('ERROR!', 'NO TAKE SELECTED : Please select a take');
    }
}//end ILsetCircled()

/// Set the new take's result to "Not Circled"
function ILsetNotCircled() 
{
    //DEBUG insole.warn("setting to Not Circled");
    
    if(tree && tree.getSelectionModel().getSelectedNode()) 
    {//check that an item/take is selected
	document.getElementById('resultText').innerHTML = "Not Circled";
	document.getElementById('resultText').className = "notCircled";
    }
    else
    {
          Ext.MessageBox.alert('ERROR!', 'NO TAKE SELECTED : Please select a take');
    }
}//end ILsetNotCircled

//call the correct popup form for the node on form button call
function ILformCall (ILbuttonText)
{
    //DEBUG insole.warn("edit function called");
    var seriesSelected = ILcheckSeriesSelected();
    if (seriesSelected)
    {
        var progSelected = ILcheckProgSelected();
        if (progSelected)
        {
            if(ILbuttonText == 'new')
            {
                ILformType = "new";
                ILcreateFormWindow('itemForm');
            }
            else //is an edit, pick up or invalid
            {
                ILtempNode = null;
                ILtempNode = tree.getSelectionModel().getSelectedNode();
                if(tree && ILtempNode) 
                {
                    var nodeDepth = ILtempNode.getDepth(); 
                    //DEBUG insole.warn("edit of node called on "+ILtempNode+" with depth "+nodeDepth);
                    if (nodeDepth == 1)//is an item
                    {
                        if(ILbuttonText == 'edit')
                        {
                            ILformType = "edit";
                        }
                        else if(ILbuttonText == 'pickup')
                        {
                            ILformType = "pickup";
                        }
                        else
                        {
                            ILformType = null;
                        }
                        ILcreateFormWindow('itemForm');
                    }
                    else if (nodeDepth>1)  //is a take or deeper, negative or 0
                    {
                        if(ILbuttonText == 'pickup')
                        {
                            ILformType = "pickup";
                            //get the current item or parent from takes
                            while (nodeDepth > 1)
                            {
                                ILtempNode = ILtempNode.parentNode;
                                nodeDepth = ILtempNode.getDepth();
                            }
                            //node is a take, just get parent id
                            ILcurrentItemId = ILtempNode.id;
                            ILcreateFormWindow('itemForm');
                        }
                        else if(ILbuttonText == 'edit')
                        {
                            ILformType = "edit";
                            ILcreateFormWindow('takeForm');
                        }
                        else
                        {
                            ILformType = null;
                            Ext.MessageBox.alert('ERROR in creating Form!', 'Unable to create Form - invalid form type supplied : Please check selection');
                        }
                    }//end is a take or deeper
                    else
                    {
                        ILformType = null;
                        Ext.MessageBox.alert('ERROR in creating Form!', 'Unable to create Form - invalid node : Please check selection');
                    }
                }//end a tree exist and a node selected
                else
                {
                    ILformType = null;
                    Ext.MessageBox.alert('ERROR in creating Form!', 'Unable to create Form - NOTHING SELECTED : Please select something first');
                }
            }//end is other than 'new' form type
        }//end programme selected
    }//end series selected
}//end ILformCall()

function ILswitchFormResult()
{
    if (document.getElementById('circledTake').checked)
    {
        document.getElementById('circledButton').className = 'circledTakeForm';
        document.getElementById('notCircledButton').className = 'unselectedResult';
    }
    else
    {
        document.getElementById('circledButton').className = 'unselectedResult';
        document.getElementById('notCircledButton').className = 'notCircledTakeForm';
    }
}//end ILswitchFormResult()

//return the edit take form html for popup
function ILtakeFormDisplay(ILtakeData)
{
    var ILtableStart = "<div id='takeFormWindow' class='takeInfoFormElement'>"+
                       "<table class='alignmentTable'> <tbody> <tr>"+
                       "<td>Item : <span class='takeFormNameAndNum'>"+ILtempNode.parentNode.attributes.itemName+
                       "</span></td> </tr> <tr> "+
                       "<td> Take Number :<span class='takeFormNameAndNum'>"+ILtempNode.attributes.takeNo+
                       "</span></td> </tr> <tr> <td> <table class = 'alignmentTable'> <tbody> <tr> <td>"+
                       " <table class = 'alignmentTable'> <tbody> <tr> <td>Result : "+
                       "<form> <table class='alignmentTable'> <tbody> <tr>";

    if (ILtempNode.attributes.result == "Circled")
    {
        ILradioButtons = "<td><input type='radio' name='takeResult' onclick='ILswitchFormResult()' checked='checked' id ='circledTake'/></td>"+
        "<td><div id='circledButton' class='circledTakeForm'> Circled </div></td>"+
        "</tr><tr>"+
        "<td><input type='radio' id ='badTake'  name='takeResult' onclick='ILswitchFormResult()' /></td>" +
        "<td><div id='notCircledButton' class='unselectedResult'>   Not Circled</div></td>";
    }
    else
    {
        ILradioButtons = " <td><input type='radio' name='takeResult' id ='circledTake' onclick='ILswitchFormResult()'/></td>"+
                        "<td><div id='circledButton' class='unselectedResult'> Circled </div></td>"+
                        "</tr><tr>"+
                        "<td><input type='radio' name='takeResult' id ='badTake' checked='checked' onclick='ILswitchFormResult()' /></td>"+
                        "<td><div id='notCircledButton' class='notCircledTakeForm'> Not Circled</div></td>";

    }
    
    var ILtableClose = " </tr> </tbody> </table> </form> </td> </tr> <tr><td>Location : "+ILtempNode.attributes.location+
                        "</td></tr> </tbody> </table> </td> <td><table class = 'alignmentTable'> <tbody> <tr>"+
                        " <td>Comment:</td><td> <textarea id='editTakeCommentBox'cols='75' rows='6'>"+ILtempNode.attributes.comment+
                        "</textarea>  </td> </tr> </tbody> </table> </td> </tr> </tbody> </table> </tr>"+
                        "<tr><td>Date : "+ILtempNode.attributes.date+"</td></tr><tr><td>Inpoint : "+
                        ILtempNode.attributes.inpoint+"</td></tr><tr><td>Outpoint : "+
                        ILtempNode.attributes.out+"</td></tr><tr><td>Duration : "+
                        ILtempNode.attributes.duration+"</td></tr>"+
                        "<tr><td><form><input type='button' class='buttons' id='updateButton' value='Update' onclick='ILprocessTakeForm()' /> &nbsp <input type='button' class='buttons' id='cancelButton' value='Cancel' onclick='ILhideForm()' /></form></td></tr></tbody></table></div>";
    
    var title ="EDIT TAKE";        
    var content =  ILtableStart + ILradioButtons + ILtableClose;
    //standard wrapping to the individual content
    document.getElementById('takeForm').innerHTML = "<h1>"+ title +"</h1><br /><div id='itemInputError'></div>" + content +
                                        "<div class='spacer'>&nbsp;</div>";
    ILwinForm.show();

    
}//end ILtakeFormContent()

function ILtakeFormCreate()
{
    ILtempNode = tree.getNodeById(ILcurrentItemId);
    var dbId = ILtempNode.attributes.databaseId;
    //requires a database call
     Ext.Ajax.request({
            url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getSingleTake.pl',
            params : {
                id: dbId
            },
            success: function(response) {
                try {
                    //store data to global temp variable/object
                    var data = JSON.parse(response.responseText);
                    ILtakeFormDisplay(data);
                } catch (e) {
                    insole.log("JSON parse error: "+e.name+" - "+e.message);
                    insole.log("JSON data was: "+response.responseText);
                    insole.alert("Failed to process data from database for item with id: "+dbId+"See previous log message.");
                }
            },
            failure: function() {
                insole.alert("Failed to get data from database for item with id: "+dbId);
            }
        });
}//end ILtakeFormCreate

//return the correct html for the new, edit and pickup item popup
function ILitemFormDisplay(itemVal, seqValStart, seqValEnd)
{
    var ILitemFormStart = "<div id='itemFormWindow' class='itemInfoFormElement'>"+
    "<table class='alignmentTable'>"+
    "<tr>   <th><div id='itemNameInputHeading' class='itemPopupHeading'>Item Info</div></th>"+
    "<td><div id='itemInputText'><input type='text' name='itemName'id='itemBox' class='itemPopupInput'";
    var ILitemNameValue = "value ='"+itemVal+"'";
    var ILitemFormContinued = "></div>  </td> </tr>"+
                    "<tr> <th><div id='seqInputHeading' class='itemPopupHeading'>Dialogue Number</div></th>"+
                    "<td><div id='seqInputText'>FROM:<input type='text' name='seq' id='seqBoxStart' class='itemPopupHeading'";
    var ILseqStartValue = "value='"+seqValStart+"'";
    var ILitemSandE= "> </td><td>TO:<input type='text' name='seq' id='seqBoxEnd' class='itemPopupHeading'"; 
    var ILseqEndValue = "value='"+seqValEnd+"'";
    var ILitemFormToButtonsLayout ="></div></td></tr>"+
                          "<tr> <td> <div class='spacer'>&nbsp;</div> </td> </tr> <tr> <td>";
    var title = "";
    var ILbuttonFunctionAndText = "";
    if(ILformType == "edit")
    {
        title="EDIT ITEM";
//         ILbuttonFunctionAndText = "<a class='simpleButton' href='javascript:ILprocessItemForm(\"update\");'>Update</a>";
           ILbuttonFunctionAndText = "<input type='button' class='buttons' id='updateButton' value='Update' onclick='ILprocessItemForm(\"update\")' /> &nbsp ";
    }
    else if (ILformType == "pickup")
    {
        title="PICKUP ITEM";
//         ILbuttonFunctionAndText = "<a class='simpleButton' href='javascript:ILprocessItemForm(\"new\");'>Add Pickup</a>";
           ILbuttonFunctionAndText = "<input type='button' class='buttons' id='updateButton' value='Update' onclick='ILprocessItemForm(\"new\")' /> &nbsp ";
    }
    else if (ILformType == "new")
    {
        title="NEW ITEM";
//         ILbuttonFunctionAndText = "<a class='simpleButton' href='javascript:ILprocessItemForm(\"new\");'>Add Item</a>";
           ILbuttonFunctionAndText = "<input type='button' class='buttons' id='updateButton' value='Update' onclick='ILprocessItemForm(\"new\")' /> &nbsp ";
    }
    else
    {
        ILbuttonFunctionAndText = "<form>INVALID FORM TYPE CALLED";
    }

//      var ILcancelButtonAndTableClose = "</td><td><a class='simpleButton' href='javascript:ILhideForm();'>Cancel</a></td></tr> </table></div>";
      var ILcancelButtonAndTableClose = "<input type='button' class='buttons' id='cancelButton' value='Cancel' onclick='ILhideForm()' /> &nbsp </form></td></tr> </table></div>";
      var content = ILitemFormStart + ILitemNameValue + ILitemFormContinued + ILseqStartValue + ILitemSandE + ILseqEndValue + ILitemFormToButtonsLayout +
               ILbuttonFunctionAndText + ILcancelButtonAndTableClose;
      
      //standard wrapping to the individual content
      document.getElementById('itemForm').innerHTML = "<h1>"+ title +"</h1><br /><div id='itemInputError'></div>" + content +
                                                 "<div class='spacer'>&nbsp;</div>";
      ILwinForm.show();
}//end itemFormDisplay()

function ILitemFormCreate()
{
    if(ILformType == "new")
    {
        ILitemFormDisplay("","","");
    }
    else if (ILformType == "edit" || ILformType == "pickup")
    {
        ILtempNode = tree.getNodeById(ILcurrentItemId);
        var dbId = ILtempNode.attributes.databaseId;
        //requires a database call
         Ext.Ajax.request({
                url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getSingleItem.pl',
                params : {
                    id: dbId
                },
                success: function(response) {
                    try {
                        //store data to global temp variable/object
                        var data = JSON.parse(response.responseText);
                        var seqArrayLen = data.SEQUENCE.length; //gives number of elements in seq array
                        var namePass = data.ITEMNAME;
                        var namePass = data.ITEMNAME;
                        if(ILformType == "pickup")
                        {
                            namePass = data.ITEMNAME+'-pickup';
                        }
                        ILitemFormDisplay(namePass,data.SEQUENCE[0],data.SEQUENCE[(seqArrayLen-1)]);
                    } catch (e) {
                        insole.log("JSON parse error: "+e.name+" - "+e.message);
                        insole.log("JSON data was: "+response.responseText);
                        insole.alert("Failed to process data from database for item with id: "+dbId+"See previous log message.");
                    }
                },
                failure: function() {
                    insole.alert("Failed to get data from database for item with id: "+dbId);
                }
            });
    }
    else
    {
          insole.alert("Unknown form type was passed to function, unable to create form");
    }
}//end ILitemFormCreate()

//the popup window
var ILwinForm;
//creates and shows the popup form
function ILcreateFormWindow (ILid)
{
    ILwinForm = new Ext.Window({
        contentEl: ILid,
        closeAction:'hide',
        plain: true,
        modal: true,
        resizable: false,
        width: 800          // need to specify dimensions in IE
    });
    if (ILid == "takeForm")
    {
          ILtakeFormCreate();
    }
    else if (ILid == "itemForm")
    {
          ILitemFormCreate();
    }//end if item form
    else
    {
        insole.alert("Unknown form id was passed to function, unable to create form");
    }

}//end ILcreateFormWindow()

//hide form and clear divs
function ILhideForm ()
{
    ILtempNode = null;
    ILformType = null;
    ILwinForm.hide();
    document.getElementById('takeForm').innerHTML = "";
    document.getElementById('itemForm').innerHTML = "";
    
}//end ILHideForm()

//process the committed data from item form
function ILprocessItemForm(ILprocessType)
{
    var ILnewItemName = document.getElementById('itemBox').value;
    var ILnewStartSeq = document.getElementById('seqBoxStart').value;
    var ILnewEndSeq = document.getElementById('seqBoxEnd').value;
   
    if (ILnewItemName == "")
    {
        ILnewItemName = "Default Item Name";
    }
    var ILvalidInput = ILvalidateItemInput(ILnewItemName, ILnewStartSeq, ILnewEndSeq);
    if(ILvalidInput)
    {
        //valid input will not allow both empty if one is empty replace it with 0 as non-digit not allowed
        if (ILnewStartSeq=="")
        {
            ILnewStartSeq= "0";
        }
        if (ILnewEndSeq=="")
        {
            ILnewEndSeq= "0";
        }
        var ILnewSeq = ILnewStartSeq+","+ILnewEndSeq;
        if(ILprocessType == "update")
        {
          //process changes, update if necessary and then close the window
          ILtempNode = tree.getNodeById(ILcurrentItemId);
          var dbId = ILtempNode.attributes.databaseId;
          var ILoldItemName = ILtempNode.attributes.itemName;
          var ILoldSeq = ILtempNode.attributes.sequence;
          //if there are any changes
          if(ILnewItemName != ILoldItemName || ILnewSeq != ILoldSeq)
          {
                //update item name and seq
                ILupdateItemInDb(ILnewItemName, ILnewSeq, dbId);
                //DEBUG 
                insole.warn("sending "+ILnewItemName+" "+ILnewSeq+" "+dbId);
            }   
            
            //ILitemUpdate(ILnewItemName, ILnewSeq);
        }
        else if(ILprocessType == "new")
        {
            ILstoreItem(ILnewItemName, ILnewSeq);
        }
        else
        {
           insole.alert("ERROR: Unknown form type used!");
        }
        ILhideForm();  
    }//end is validInput
}//end ILprocessItemForm()

//process commited data take form popup
function ILprocessTakeForm ()
{
    //check whether the result or the comment has changed and if it is different call the script to change the values
    ILtempNode = tree.getSelectionModel().getSelectedNode();
    var ILcurrentResult = ILtempNode.attributes.result;
    var ILcurrentComment = ILtempNode.attributes.comment;
   
    var dbId = ILtempNode.attributes.databaseId;
    var ILnewComment = document.getElementById('editTakeCommentBox').value;
    
    if(document.getElementById('circledTake').checked == true)
    {
        var ILnewResult = "Circled";
    }
    else
    {
        var ILnewResult = "Not Circled";
    }
    if(ILnewResult != ILcurrentResult || ILnewComment != ILcurrentComment)
    {
        //update result
        //DEBUG 
        insole.warn("updating take");
        ILupdateTakeInDb(ILnewResult, ILnewComment, dbId);
      //set it to expand the takes parent item if a change has been made
        ILtempNode = ILtempNode.parentNode;
        ILcurrentItemId = ILtempNode.id;
    }
    ILhideForm();
}//end ILprocessTakeUpdate ()

/// Populate a list of available recorders from database information
var ILRecorders = new Object();
function ILdiscoverRecorders() 
{
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/discoverRecorders.pl',
		success: function(response){
			try {
				var nodes = JSON.parse(response.responseText);
			} catch (e) {
				insole.alert("Error loading recorder list");
			}
			var elSel = document.getElementById('recorderSelector');
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
			//number of recorders
			numOfRecs = 0;
			
			//add recorders
			for (recNode in nodes) 
			{
				if(nodes[recNode].nodeType != "Recorder") continue;
				var elOptNew = document.createElement('option');
				elOptNew.text = recNode;
				elOptNew.value = numOfRecs;
				numOfRecs++;
				try {
					elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
				} catch (e) {
					elSel.add(elOptNew); // IE only
				}
			}
			ILRecorders = nodes;
			
			//add system time options
			for (var i=0; i<numOfSysTimeOpts; i++)
			{			
				var elOptNew = document.createElement('option');
				if(i==0){elOptNew.text = "25fps Local Time";}
				if(i==1){elOptNew.text = "29.97fps non-drop Local Time";}
				if(i==2){elOptNew.text = "29.97fps drop Local Time";}
				if(i==3){elOptNew.text = "30fps non-drop Local Time";}
				elOptNew.value = (numOfRecs + i);
				try{
					//DEBUG insole.warn("editrate set to ="+editrate);
					elSel.add(elOptNew, null); //standards compliant; doesn't work in IE
				}catch (e){
					elSel.add(elOptNew); //IE only
				}
			}//end for systime opts			
		}, //end success
		failure: function()	{
			insole.alert("Error loading recorder list");
		}
	});
}//end ILdiscoverRecorders()

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

//compare strings for sorting of dropdown lists (perl hash returns unsorted)
function ILcompareNames(a,b)
{
    if (a[0] > b[0])
    {
        return 1;
    }
    else if (a[0] < b[0])
    {
        return -1;
    }
    else //a[0]==b[0]
    {
        return 0;
    }
    
}//end compare names in array and sort accordingly

/// Grab the series and location lists from the database
function ILpopulateSeriesLocInfo() 
{
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgRecInfo.pl',
		success: function(response){
			try {
				var data = JSON.parse(response.responseText);
			} catch (e) {
				insole.alert("Error loading series, programme & recording location lists");
			}
			
			//need to sort the information from data as is not in a sorted form
			var elSel = document.getElementById('recLocSelector');
			elSel.options.length=0;
						
			var genArray = new Array();
			var genIndexCount = 0;
			
			for (var loc in data.recLocs)  
            {
			    genArray.push([data.recLocs[loc], loc]);
			    genIndexCount++;
            }
			
			//sort the array into alphabetical order
			genArray.sort(ILcompareNames);
            
			for (var i = 0; i <genIndexCount ; i++)  
            {
                var elOptNew = document.createElement('option');
                elOptNew.text = genArray[i][0];
                elOptNew.value = genArray[i][1];
                try {
                    elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
                } catch (e) {
                    elSel.add(elOptNew); // IE only
                }
            }
            
			//clear array
			genArray.length = 0;
			//reset index count to zero
			genIndexCount = 0;
						
			elSel = document.getElementById('seriesSelector');
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

			for (var series in data.series)  
            {
                genArray.push([data.series[series].name, series]);
                genIndexCount++;
            }
            
            //sort the array into alphabetical order
            genArray.sort(ILcompareNames);
            
            for (var i = 0; i <genIndexCount ; i++)  
            {
                var elOptNew = document.createElement('option');
                elOptNew.text = genArray[i][0];
                elOptNew.value = genArray[i][1];
                try {
                    elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
                } catch (e) {
                    elSel.add(elOptNew); // IE only
                }
            }
        	           
			ILseriesData = data.series;
			//make programme selector unavailable until a series is selected
			document.getElementById('programmeSelector').disabled = true;
                        
		},
		failure: function()	{
			insole.alert("Error loading series, programme & recording location lists");
		}
	});
}//end ILpopulateSeriesLocInfo

///Populate the list of programmes based on the selected series
function ILupdateProgrammes() 
{
   //DEBUG  insole.warn("update called");
    ILresetItemName();
    
    var seriesSel = document.getElementById('seriesSelector');
    var seriesID = seriesSel.options[document.getElementById('seriesSelector').selectedIndex].value;
    seriesSel.style.color = "rgb(0,0,0)";
    
    //DEBUG insole.warn("after style change");
    //as the series has changed reset the programmes and reload the tree
	document.getElementById('programmeSelector').selectedIndex = 0;
	ILrefreshTree();
	
	//reset the programme
	var elSel = document.getElementById('programmeSelector');
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

        document.getElementById('currentItemName').innerHTML = "No Item Selected";
        document.getElementById('currentItemName').className = 'itemNameNone';
        document.getElementById('currentTakeNum').innerHTML = "-";
        document.getElementById('currentTakeNum').className = 'itemNameNone';

	if (seriesID == -1){
		//disable the programme item selectors and set them to show nothing
		document.getElementById('programmeSelector').disabled=true;
		document.getElementById('programmeSelector').selectedIndex = 0;
	}
	else
	{   //a series is selected
            //enable the programme selector
            document.getElementById('programmeSelector').disabled=false;
            var programmes = ILseriesData[seriesID].programmes;

            //JSON does not returned an ordered list, sort by alphabetical and then add to drop down
            var genArray = new Array();
            var genIndexCount = 0;
            
            for (var prog in programmes) 
            {
                genArray.push([programmes[prog], prog]);
                genIndexCount++;
            }
            
            //sort the array into alphabetical order
            genArray.sort(ILcompareNames);
            
            for (var i = 0; i <genIndexCount ; i++)  
            {
                var elOptNew = document.createElement('option');
                elOptNew.text = genArray[i][0];
                elOptNew.value = genArray[i][1];
                try {
                    elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
                } catch (e) {
                    elSel.add(elOptNew); // IE only
                }
            }
            
            //clear array
            genArray.length = 0;
            //reset index count to zero
            genIndexCount = 0;
       }//a series is selected
	document.getElementById('seriesSelector').blur();
}//end ILupdateProgrammes

/// Grab the selected programme's items/takes info from the database (refresh tree and reset item name)
function ILloadNewProgInfo () 
{
        ILresetItemName();
        //ensure clears any error colouring may have
        document.getElementById('programmeSelector').style.color = "rgb(0,0,0)";
        //if no longer have items available get rid of take display
            document.getElementById('currentItemName').innerHTML = "No Item Selected";
            document.getElementById('currentItemName').className = 'itemNameNone';
            document.getElementById('currentTakeNum').innerHTML = "-";
            document.getElementById('currentTakeNum').className = 'itemNameNone';
        //automatically clears previous nodes on load by default
	//url from which to get a JSON response
	ILtreeLoader.url = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgramme.pl';
	//load the info to a root node
	ILtreeLoader.load(tree.getRootNode());
        document.getElementById('programmeSelector').blur();
}//end ILloadNewProgInfo

//just refreshes the tree without resetting the selected item
function ILrefreshTree ()
{
    //url from which to get a JSON response
   // ILtreeLoader.url = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgramme.pl';
    ILtreeLoader.load(tree.getRootNode());
    
}//end ILrefreshTree

function ILcallbackDownloadPdf(response)
{
    //DEBUG    
    insole.warn("Passed parameters to pdf");
    insole.warn("Now creating download link");
        try{
            var filenameData = JSON.parse(response.responseText);
            
            var message = '';
            message += "Download PDF file: " + ILfileLink(filenameData.filename) + "<BR>";
            
            Ext.MessageBox.alert("Created Log Sheet", message);
        }
        catch(e){
            jsonParseException(e);
            insole.alert("Unable to Parse JSON returned from request");
        }
    
}//end ILcallbackDownloaPdf()

//on success response from get items calls this function
function ILcallbackPrintData(response)
{
     try {
            var progData = JSON.parse(response.responseText);
            //have all items and takes for this programme
        } catch (e) {
            insole.log("JSON parse error: "+e.name+" - "+e.message);
            insole.log("JSON data was: "+response.responseText);
            insole.alert("Failed to load programme from database.");
        }
        
    /* 
     * progData contains all items and takes
     * Here is where you would perform options tasks such as only print marked/circled takes and elements to be included etc.
     */
    var selectedIndex = document.getElementById('seriesSelector').selectedIndex;
    var seriesName = document.getElementById('seriesSelector').options[selectedIndex].text;
    selectedIndex = document.getElementById('programmeSelector').selectedIndex;
    var progName = document.getElementById('programmeSelector').options[selectedIndex].text;
    
    var json = {
                "DataRoot": progData,
                "Programme": progName,
                "Series": seriesName
                };
    var jsonText = JSON.stringify(json);
    //from material module, should probably use the same ext ajax as rest of module for consistency
    //var dataUrl = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/createPDFLogSheet.pl';
    //var params = 'jsonIn=' + progData;
    //ajaxRequest(dataUrl, "POST", params, ILcreatePDFCallback);
    //make a new request to pdf script passing JSON object with all parameters in it
    
  //get items and takes from database
    Ext.Ajax.request({
        url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/createPDFLogSheet.pl',
        params : {
        jsonIn: jsonText
    },
    success: ILcallbackDownloadPdf,
    failure: function() {
        insole.alert("Failed to to load Programme from database.");
    }
 });
    
    
}//end ILcallbackPrintData()

///create a pdf version of the logging data
function ILpdf ()
{
    //perform a check to ensure that a series and programme are selected before an attempt to add a new item is made
    var seriesSelected = ILcheckSeriesSelected();
    if (seriesSelected)
    {
        var progSelected = ILcheckProgSelected();
        if (progSelected)
        {
            var selectedIndex = document.getElementById('programmeSelector').selectedIndex;
            var progId = document.getElementById('programmeSelector').options[selectedIndex].value;

            //get items and takes from database
            Ext.Ajax.request({
                url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgramme.pl',
                params : {
                progid: progId 
            },
            success: ILcallbackPrintData,
            failure: function() {
                insole.alert("Failed to to load Programme from database.");
            }
            });
        }//if programme selected
    }//if series selected
}//end ILpdf()

/*
 * creates an html link to an output file
 */
function ILfileLink(filename){
    var filenameText = addSoftHyphens(filename);
    var html = "<a href=" + "/cgi-bin/ingex-modules/Logging.ingexmodule/download.pl?fileIn=" + filename + ">" + filenameText + "</a>";
    return html;
}


/*
 * add soft hyphens to a long piece of text (eg urls) so it can be split
 */
function ILaddSoftHyphens(text){
    var next;
    var out = "";
    
    for(var i=0; i<text.length; i+=1){
        next = text.substring(i,i+1);
        out += "&shy;"+next;
    }   
    
    return out;
}


/// Open a printable version of the data
function ILprint () 
{
    var seriesSelected = ILcheckSeriesSelected();
    if (seriesSelected)
    {
        var progSelected = ILcheckProgSelected();

        if (progSelected)
        {
            //expand all items to ensure they are included in the print
            ILexpandItems();

            var series = document.getElementById('seriesSelector').options[document.getElementById('seriesSelector').selectedIndex].text;
            var programme = document.getElementById('programmeSelector').options[document.getElementById('programmeSelector').selectedIndex].text;
            var now = new Date();
            var monthname=new Array("Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec");
            var date = now.getDate()+" "+monthname[now.getMonth()]+" "+now.getFullYear();
            var newwindow=window.open('','ILprintWindow','height=400,width=980,scrollbars=1,resizable=1');
            var tmp = newwindow.document;
            tmp.write('<html><head><title>Ingex Log</title>');
            tmp.write('<link rel="stylesheet" href="/ingex/printpage.css">');
            tmp.write('</head><body><div id="progInfoDiv" style="float:left">');
            tmp.write('<div class="date">Date: '+date+'</div>');
            //tmp.write('<div class="producer">Producer: '+document.getElementById('producer').value+'</div>');
            //tmp.write('<div class="director">Director: '+document.getElementById('director').value+'</div>');
            //tmp.write('<div class="pa">PA: '+document.getElementById('pa').value+'</div></div>');
            tmp.write('<h1>'+series+'</h1>');
            tmp.write('<h2>'+programme+'</h2>');
            tmp.write('<div style="clear:both">&nbsp;</div>');
            tmp.write('<table><tr class="header"><th>TAKE</th><th colspan=2>TIMECODE</th><th>COMMENT</th>');
            var items = tree.getRootNode().childNodes;
            for(var i in items) 
            {
                if(typeof items[i].attributes != "undefined") 
                {
                    tmp.write('<tr class="itemHeaderLine"><td colspan="7">'+items[i].attributes.itemName+'</td></tr>');
                    if (items[i].firstChild != null)
                    {
                        var takes = items[i].childNodes;
                        for(var t in takes) 
                        {
                            if (typeof takes[t].attributes != "undefined") 
                            {
                                var a = takes[t].attributes;
                                tmp.write('<tr>');
                                tmp.write('<td class="take">'+a.takeNo+'<br />'+a.result+'</td>');
                                tmp.write('<td class="tclabel">in:<br />out:<br />dur:<br />date:</td>');
                                tmp.write('<td class="timecode">'+a.inpoint+'<br />'+a.out+'<br />'+a.duration+'<br />'+a.date+'</td>');
                               //tmp.write('<td class="result">'+a.result+'</td>');
                                tmp.write('<td class="comment">'+a.comment+'</td>');
                               // tmp.write('<td class="location">'+a.location+'</td>');
                               // tmp.write('<td class="date">'+a.date+'</td>');
                                tmp.write('</tr>');
                            }
                        }
                    }//end item has cildren
                    else
                    {
                        tmp.write('<tr>');
                        tmp.write('<td class="take">NO TAKES</td>');
                        tmp.write('<td class="tclabel"><br /><br /></td>');
                        tmp.write('<td class="timecode"><br /><br /></td>');
                        tmp.write('<td class="comment">NO TAKES HAVE BEEN MADE<br /></td>');
                        //tmp.write('<td class="date"></td>');
                        tmp.write('</tr>');
                    }
                }//end item exists and has attributes
            }//end for all items
            tmp.write('</table></body></html>');
            tmp.close();
            newwindow.print();
            //expand currently selected item
            ILexpandSingleItem();
        }//programme selected
    }//series selected
}//end ILprint


/// Import a script (currently un-implemented)
function ILimport () {
	Ext.MessageBox.alert('Import Unavailable', 'Unfortunately, this feature has not yet been implemented.');
}//end ILimport

/// A "class" for a timecode
function ingexLoggingTimecode () 
{
	
	//holds the value of the currently selected
	var recSel = document.getElementById('recorderSelector');
	
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
	this.incrementMethod = function()
	{
	    recSel.style.color = "rgb(0,0,0)";
	    //if no timecode supply selected do nothing
		if (recSel.options[recSel.selectedIndex].value == -1){
		 	clearInterval(ILtc.incrementer);
			ILtc.incrementer = false;
			clearTimeout(ILtc.ajaxtimeout);
			ILtc.ajaxtimeout = false;
			dropFrame = false;
			document.getElementById('tcDisplay').innerHTML = "--:--:--:--";
		}
		else if (recSel.options[recSel.selectedIndex].value >= numOfRecs) //if system time selected
		{ 
			if(document.getElementById('tcDisplay').innerHTML == "--:--:--:--" ) {
					showLoading('tcDisplay');
			}
				
			//clear incrementer and begin updating from local system time
			clearInterval(ILtc.incrementer);
			ILtc.incrementer = false;
			clearTimeout(ILtc.ajaxtimeout);
			ILtc.ajaxtimeout = false;
			
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
			if(recSel.options[recSel.selectedIndex].value == numOfRecs + 2){//29.97 drop (TODO ensure that dropframe is working correctly)
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
			
		}//end if a system time option selected
		else //if a recorder is selected
		{ 
			if(document.getElementById('tcDisplay').innerHTML == "--:--:--:--") 
			{
				showLoading('tcDisplay');
			}
			clearInterval(ILtc.incrementer);
			ILtc.incrementer = false;
			clearTimeout(ILtc.ajaxtimeout);
			ILtc.ajaxtimeout = false;
			//make a request to update the edit rate			
			var xmlHttp = getxmlHttp();
			xmlHttp.onreadystatechange = function () {
				//waits until response content is loaded before doing anything
				if (xmlHttp.readyState==4) {
					
					if (ILtc.ajaxtimeout) {
						clearTimeout(ILtc.ajaxtimeout);
						ILtc.ajaxtimeout = false;
					}
				
					try {
						var tmp = JSON.parse(xmlHttp.responseText);
						var data = tmp.tc;
						ILtc.setEditrate(data.frameNumer,data.frameDenom);
						
					} catch (e) {
						clearInterval(ILtc.incrementer);
						insole.alert("Received invalid response when updating timecode (JSON parse error). Timecode display STOPPED.");
						insole.log("Exception thrown:   Name: "+e.name+"   Message: "+e.message);
						document.getElementById('tcDisplay').innerHTML = "<span class='error'>Error</span>";
						Ext.MessageBox.alert('ERROR getting recorder URL!', 'Unable to connect to recorder : Please make sure Ingex is running and check status\n Capture process and nexus_web required to receive timecode');
					}
					//calling start function
					ILtc.startRecIncrement();
				}
			};

			//only search for a recorder url if on the Logging tab
			if (currentTab == "Logging")
			{
			    //if a recorder is selected
				if (recSel.options[recSel.selectedIndex].value != -1 
				        && recSel.options[recSel.selectedIndex].value < numOfRecs )
				{  
					var recorderURL = ILgetRecorderURL(recSel.options[recSel.selectedIndex].text);
					
					if(!recorderURL){
						insole.alert("Error querying recorder for timecode - no recorder URL available");
						return false;
					}
			
					var now = new Date();
					var random = Math.floor(Math.random()*101);
					var requestID = now.getHours()+now.getMinutes()+now.getSeconds()+now.getMilliseconds()+random;
					//make a request to server
					xmlHttp.open("GET","/cgi-bin/ingex-modules/Logging.ingexmodule/tc.pl?url="+recorderURL+"&"+requestID,true);
					xmlHttp.send(null);
					//if this is set clear it
					if (this.ajaxtimeout) {
						clearTimeout(this.ajaxtimeout);
					}
					//if server doesn't respond in time then times out
					this.ajaxtimeout = setTimeout( function(){ ILtc.ajaxTimedout(); } , 5000);
				}
				
			}//end if on logging tab
		}//end a recorder is selected
                document.getElementById('recorderSelector').blur();
	};//end this.incrementMethod = function()
	
	this.startRecIncrement = function(){
		ILtc.startIncrementing(true);//true for recorder selected
	};

	//updates the timecode from a given framecount
	this.updateTimecodeFromFrameCount = function(frameCount)
	{
		var tcEditRate;
		if(editrate == ntscFps )
		{
			if(!dropFrame) //not using dropframe
			{
				tcEditRate = Math.round(editrate);//eg 29.97 is in base 30 timecode
			}
			else //is dropframe
			{
			    tcEditRate = editrate;
		    }
		}
		else //Not 29.97 fps
		{
			tcEditRate = editrate;
		}
		
		if (!dropFrame)
		{
			//this is correct for any non drop frame rate 
			this.s = Math.floor(frameCount/tcEditRate);
			this.m = Math.floor(this.s/60);
			this.h = Math.floor(this.m/60);
			this.f = Math.floor(frameCount - (tcEditRate * this.s));
			this.s = this.s - (60 * this.m);
			this.m = this.m - (60 * this.h);
						
		}
		else //is drop frame
		{
		    //TODO ensure dropframe is working correctly
			//if using system time or just incrementing then need to work out the timecode from framecount
			var countOfFrames = frameCount;
			this.h = Math.floor( frameCount / FRAMES_PER_DF_HOUR);
			countOfFrames = countOfFrames % FRAMES_PER_DF_HOUR;
			var ten_min = Math.floor(countOfFrames / FRAMES_PER_DF_TEN_MIN);
			countOfFrames = (countOfFrames % FRAMES_PER_DF_TEN_MIN);
			
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
		//DEBUG insole.warn("within update from count h= "+this.h+" m="+this.m+"  s="+this.s+"  f="+this.f);
		//DEBUG insole.warn("frameCount="+frameCount+" minus this"+(editrate * this.s));
	};// end this.updateTimecodeFromFrameCount = function(frameCount)

	//returns number of frames since midnight from a passed date object
	this.getFramesSinceMidnightFromDate = function (dateObj)
	{
		var h; var m; var s; var f;
		h = dateObj.getHours();
		m = dateObj.getMinutes();
		s = dateObj.getSeconds();
		f = Math.floor((editrate/1000)*dateObj.getMilliseconds());
		//returns the total number of frames since midnight
		return (Math.floor(h*60*60*editrate) + Math.floor(m*60*editrate) + Math.floor(s*editrate) + f);
	};//end this.getFramesSinceMidnightFromDate = function (dateObj)
	
	//starts incrementing the correct method
	this.startIncrementing = function(recorderSelected) 
	{
		//if no incrementer set
	    if(!this.incrementer)
		{
			if(recorderSelected) //getting update from server timecode
			{	
			    //update timecode from server at start
				this.update();
				
				// Format as 2 digit numbers
				var h; var m; var s; var f;
				if (this.h < 10) { h = "0" + this.h; }else{h = this.h;};
				if (this.m < 10) { m = "0" + this.m; }else{m = this.m;}
				if (this.s < 10) { s = "0" + this.s; }else{s = this.s;}
				if (this.f < 10) { f = "0" + this.f; }else{f = this.f;}
				
				if (h==NaN || m==NaN || s==NaN)
				{
					showLoading('tcDisplay');
				}
				else
				{
					//display to timecode area
					if (dropFrame)
					{
						document.getElementById('tcDisplay').innerHTML = h+";"+m+";"+s+";"+f;
					}
					else
					{
						document.getElementById('tcDisplay').innerHTML = h+":"+m+":"+s+":"+f;
					}
				}//not showing NaN
				
				//if the server update time is less than the increment time then no point setting incrementer just set a server update	
				if (serverUpdatePeriod < ( tcFreq*(1000/editrate)))
				{
					this.incrementer = setInterval(function(){ILtc.update();},serverUpdatePeriod);
				}
				else //if the server update time is longer than the increment period
				{
					//set incrementer
					//number of increments before serverCheck
					this.serverCheck = Math.floor(serverUpdatePeriod /( tcFreq*(1000/editrate)));//every serverCheck increments will update from server instead of incrementing in browser.
					//update the timecode by tcFreq frames every time 3 frames worth of time has passed
					this.incrementer = setInterval(function(){ILtc.increment(tcFreq, recorderSelected);},(tcFreq*(1000/editrate)));
					//DEBUG insole.warn("else increment ="+this.incrementer);
				}
			}//end a recorder is selected
			else //getting update from local timecode 
			{
				//get current time	
				var now = new Date();
				//get frames since midnight for current date
				this.framesSinceMidnight = this.getFramesSinceMidnightFromDate(now);
				//update timecode from frames since midnight - auto detects dropframe
				this.updateTimecodeFromFrameCount(this.framesSinceMidnight);
				
				//set increment to update the timecode  approximately every time tcFreq frames worth of time has passed
				this.incrementer = setInterval(function(){ILtc.increment(tcFreq, recorderSelected);},(tcFreq*(1000/editrate)));
			}//end update from local timecode
		}//end if no incrementer set
	};//end  this.startIncrementing = function(recorderSelected)
	
	
	//update the timecode from the server
	this.update = function() 
	{
		var xmlHttp = getxmlHttp();
		xmlHttp.onreadystatechange = function () 
		{
			if (xmlHttp.readyState==4) 
			{
				if (ILtc.ajaxtimeout) 
				{
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
					document.getElementById('tcDisplay').innerHTML = "<span class='error'>Error</span>";
				}
			}
		};
		
		//only search for a recorder if on the Logging tab
		if (currentTab == "Logging")
		{
		  //if a recorder is selected
			if (recSel.options[recSel.selectedIndex].value != -1 
			        && recSel.options[recSel.selectedIndex].value < numOfRecs )
			{  
				var recorderURL = ILgetRecorderURL(recSel.options[recSel.selectedIndex].text);
				
				if(!recorderURL)
				{
					 insole.alert("Error querying recorder for timecode - no recorder URL available");
					return false;
				}
		
				var now = new Date();
				var random = Math.floor(Math.random()*101);
				var requestID = now.getHours()+now.getMinutes()+now.getSeconds()+now.getMilliseconds()+random;
				xmlHttp.open("GET","/cgi-bin/ingex-modules/Logging.ingexmodule/tc.pl?url="+recorderURL+"&"+requestID,true);
				xmlHttp.send(null);
				if (this.ajaxtimeout) {
					clearTimeout(this.ajaxtimeout);
				}
				this.ajaxtimeout = setTimeout( function(){ ILtc.ajaxTimedout(); } , 5000);
			}//if a recorder is selected
		}// if on logging tab

	};//end this.update= funtion()
	
	this.ajaxTimedout = function() 
	{
		clearTimeout(this.ajaxtimeout);
		clearInterval(this.incrementer);
		this.ajaxtimeout = false;
		insole.alert("AJAX timeout occurred when updating timecode. Moving to system time");
		this.startIncrementing(false);
		document.getElementById('tcDisplay').innerHTML = "<span class='error'>Error</span>";
	};//end this.ajaxTimedout = function()

	this.setEditrate = function(numerFrame,denomFrame) 
	{
		editrate = numerFrame/denomFrame;
	};

	//set the current timecode from server framecount
	this.set = function(h,m,s,f,numerFrame,denomFrame,framesSinceMidnight,frameDrop,stopped) 
	{
		//update timecode
		this.h = h;
		this.m = m;
		this.s = s;
		this.f = f;
		frameNumer = numerFrame;
		frameDenom = denomFrame;
		editrate = frameNumer/frameDenom;
		this.framesSinceMidnight  = framesSinceMidnight;
		dropFrame = frameDrop;
		this.stopped = stopped;
		
		//change the duration total to match updated timecode
		if (this.takeRunning)
		{
			//work out the current duration
			this.lastFrame = this.framesSinceMidnight;
			if(this.lastFrame > this.firstFrame)//not crossing midnight
			{
				this.durFtot = this.lastFrame - this.firstFrame;
			}
			else
			{
				this.durFtot = (this.lastFrame + ((24*60*60*editrate) - this.firstFrame));
			}
		}//if a take is running
		
	};//end this.set = function(h,m,s,f,numerFrame,denomFrame,framesSinceMidnight,frameDrop,stopped) 

	this.increment = function(numToInc, recorderSelected) 
	{
		//if number of frames to increment by is not set then set to 1
		if (isDefault(numToInc)) numToInc = 1; //number of frames to increment by
		
		if(recorderSelected)//a recorder is selected
		{
			//DEBUG insole.warn("a recorder is selected")
			if (this.serverCheck == 0)//do a server update
			{
				//don't need to worry about dropframe as that is automatically provided by Timecode object
				this.update();
				//reset the serverIncrement
				this.serverCheck = Math.floor(serverUpdatePeriod /( tcFreq*(1000/editrate)));
			}
			else //count down til next server check
			{
				this.serverCheck --;
				//do the basic system time increment
				this.framesSinceMidnight +=numToInc;
				this.updateTimecodeFromFrameCount(this.framesSinceMidnight);
				//DEBUG insole.warn("serverCheck currently"+this.serverCheck);
			}
		}//end if a recorder is selected
		else //using system time options
		{
				//get current time
				var now = new Date();
				///update the frames since midnight which is used for incrementing
				this.framesSinceMidnight = this.getFramesSinceMidnightFromDate(now);
				this.updateTimecodeFromFrameCount(this.framesSinceMidnight);
			
		}//end using system time options
		
		if(this.takeRunning)
		{
			//update lastFrame of take
			this.lastFrame = this.framesSinceMidnight;
		}
		
		var fr = '--';
				
		// Format as 2 digit numbers
		var h; var m; var s; var f;
		if (this.h < 10) { h = "0" + this.h; }else{h = this.h;};
		if (this.m < 10) { m = "0" + this.m; }else{m = this.m;}
		if (this.s < 10) { s = "0" + this.s; }else{s = this.s;}
		if (this.f < 10) { f = "0" + this.f; }else{f = this.f;}
		
		if(this.f >= editrate){insole.alert("The number of frames is MORE THAN FPS");}
		if(this.f < 0){insole.alert("the number of frames is NEGATIVE");}
		// draw timecode to tcDisplay
		if(dropFrame)
		{
			if (showFrames)
			{
			    document.getElementById('tcDisplay').innerHTML = h+";"+m+";"+s+";"+f;
			}
			else
			{
			    document.getElementById('tcDisplay').innerHTML = h+";"+m+";"+s+";"+fr;
			}	
		}//end using dropframe
		else//Not using Dropframe
		{
			if (showFrames)
			{
			    document.getElementById('tcDisplay').innerHTML = h+":"+m+":"+s+":"+f;
			}
			else
			{
			    document.getElementById('tcDisplay').innerHTML = h+":"+m+":"+s+":"+fr;
			}
		}//end NOT using Dropframe
				
		//if take is runnning
		if(this.takeRunning) 
		{
			//update duration				
			//avoid odd behaviour if timecode crosses midnight
			if(this.lastFrame > this.firstFrame) //not crossing midnight
			{
				this.durFtot = this.lastFrame - this.firstFrame;
			}
			else //add number of frames in a day to the last frame and then calculate
			{
				this.durFtot = (this.lastFrame + ((24*60*60*editrate) - this.firstFrame));
			}

			//need to convert to timecode from total framecount
			//get duration timecode from number of frames
			this.durS = Math.floor(this.durFtot/editrate);
			this.durM = Math.floor(this.durS/60);
			this.durH = Math.floor(this.durM/60);
			this.durF = this.durFtot - (editrate * this.durS);
			this.durS = this.durS - (60 * this.durM);
			this.durM = this.durM - (60 * this.durH);
	
			//only needed if are adding to durF itself
			if (this.durF >= editrate) 
			{
				insole.alert("*************** duration frame number more than editrate *************");
				this.durF -= editrate;
				this.durS++;
				if (this.durS > 59) 
				{
					this.durS -= 59;
					this.durM++;
					if (this.durM > 59) 
					{
						this.durM -= 59;
						this.durH++;
					}
				}
			}//if frames is higher than allowed value
			this.durF = Math.floor(this.durF);
			
			//TODO check that all calculations are done for the drop frame timecode
			//DEBUG insole.warn("duration update = "+this.durH+":"+this.durM+":"+this.durS+":"+this.durF);
			//drop frame alterations yet to be added in
			/*if (dropFrame){
			if(this.m % 10 != 0){//if recording time elapsed is  not divisible by ten, drop first 2 frames of each minute
				if(this.f == 0 && this.s == 0){this.f = 2;}//change displayed timecode to 'drop' frames
				if(this.f == 1 && this.s == 0){this.f = 3;}
				//don't increment the total duration
				//if(this.durF == 0 && this.durS == 0){this.durF = 2}
				//if(this.durF == 1 && this.durS == 0){this.durF = 3}
				}
			}*/
		        // TODO convert duration to Min' Sec"
// 			var durMin = (this.durH * 60) + this.durM;
//                         var durSec = this.durS + 1; //just ceil to second
//                         if (durSec < 10) { durSec = "0" + durSec; }
			// Format as 2 digit numbers
			var dh; var dm; var ds; var df;
			if (this.durH < 10) { dh = "0" + this.durH; } else { dh = this.durH; }
			if (this.durM < 10) { dm = "0" + this.durM; } else { dm = this.durM; }
			if (this.durS < 10) { ds = "0" + this.durS; } else { ds = this.durS; }
			if (this.durF < 10) { df = "0" + this.durF; } else { df = this.durF; }
			
			if(dropFrame){
				if(showFrames){
					document.getElementById('outpointBox').innerHTML = h+ ";" + m + ";" + s + ";" + f;
					document.getElementById('durationBox').innerHTML = dh + ":" + dm + ":" + ds + ":" + df;
//                                         document.getElementById('durationBox').innerHTML = durMin + "' " + durSec + "\"";
				}else{
					document.getElementById('outpointBox').innerHTML = h + ";" + m + ";" + s + ";"+ fr;
					document.getElementById('durationBox').innerHTML = dh + ":" + dm + ":" + ds + ":" + fr;
//                                         document.getElementById('durationBox').innerHTML = durMin + "' " + durSec + "\"";
				}
			}else{
				if(showFrames){
					document.getElementById('outpointBox').innerHTML = h+ ":" + m + ":" + s + ":" + f;
					document.getElementById('durationBox').innerHTML = dh + ":" + dm + ":" + ds + ":" + df;
//                                         document.getElementById('durationBox').innerHTML = durMin + "' " + durSec + "\"";
				}else{
					document.getElementById('outpointBox').innerHTML = h + ":" + m + ":" + s + ":"+ fr;
					document.getElementById('durationBox').innerHTML = dh + ":" + dm + ":" + ds + ":" + fr;
//                                         document.getElementById('durationBox').innerHTML = durMin + "' " + durSec + "\"";
				}
			}
		}
	};//end this.increment

	this.startTake = function()
	{
		//if no recorder is selected
		
		//get update from server if a recorder is selected
		if (recSel.options[recSel.selectedIndex].value < numOfRecs
		        && recSel.options[recSel.selectedIndex].value != -1)
		{
			ILtc.update();
			this.firstFrame = this.framesSinceMidnight;
			this.lastFrame = this.firstFrame;
		}
		else // system time is recorded
		{ 
			this.startDate = new Date();
			this.firstFrame = ILtc.getFramesSinceMidnightFromDate(this.startDate);
			this.lastFrame = this.firstFrame;
		}
		
//                  owuld reset everything
//                 ILresetTake();

                //resets everyting except the comment in case has been entered already
                document.getElementById('inpointBox').innerHTML = "00:00:00:00";
                document.getElementById('outpointBox').innerHTML = "00:00:00:00";
                document.getElementById('durationBox').innerHTML = "00:00:00:00";
                document.getElementById('resultText').innerHTML = "Not Circled";
                document.getElementById('resultText').className = "notCircled";
		ILtc.updateTimecodeFromFrameCount(this.firstFrame);
		
		// Format as 2 digit numbers
		var h; var m; var s; var f;
		if (this.h < 10) { h = "0" + this.h; } else { h = this.h; }
		if (this.m < 10) { m = "0" + this.m; } else { m = this.m; }
		if (this.s < 10) { s = "0" + this.s; } else { s = this.s; }
		if (this.f < 10) { f = "0" + this.f; } else { f = this.f; }
		if(dropFrame)
		{
			/*document.getElementById('inpointBox').value = h + ";" + m + ";" + s + ";" + f;
			document.getElementById('outpointBox').value = h + ";" + m + ";" + s + ";" + f;*/
                        document.getElementById('inpointBox').innerHTML = h + ";" + m + ";" + s + ";" + f;
			document.getElementById('outpointBox').innerHTML = h + ";" + m + ";" + s + ";" + f;
		}
		else
		{
			document.getElementById('inpointBox').innerHTML = h + ":" + m + ":" + s + ":" + f;
			document.getElementById('outpointBox').innerHTML = h + ":" + m + ":" + s + ":" + f;
		}

		this.durH = 0;
		this.durM = 0;
		this.durS = 0;
		this.durF = 0;
		ILtc.takeRunning = true;

	};//end this.startTake

	this.stopTake = function()
	{
		//if is a recorder
		if (recSel.options[recSel.selectedIndex].value < numOfRecs 
		        && recSel.options[recSel.selectedIndex].value != -1)
		{
			ILtc.update();
			this.lastFrame = this.framesSinceMidnight;
		}
		else
		{
			this.stopDate = new Date();
			this.lastFrame = this.getFramesSinceMidnightFromDate(this.stopDate);
		}
		
		ILtc.takeRunning = false;
		
		//work out the duration
		if(this.lastFrame > this.firstFrame)//not crossing midnight
		{
			this.durFtot = this.lastFrame - this.firstFrame;
		}
		else
		{// add frames in a day to last frame before doing calculation
			this.durFtot = (this.lastFrame + ((24*60*60*editrate) - this.firstFrame));
		}
		
		//update this.h, this.m etc
		ILtc.updateTimecodeFromFrameCount(ILtc.lastFrame);
		
		//get duration time from number of frames
		this.durS = Math.floor(this.durFtot/editrate);
		this.durM = Math.floor(this.durS/60);
		this.durH = Math.floor(this.durM/60);
		this.durF = this.durFtot - (editrate * this.durS);
		this.durS = this.durS - (60 * this.durM);
		this.durM = this.durM - (60 * this.durH);
		this.durF = Math.floor(this.durF);
		
		// Format as 2 digit numbers
		var h; var m; var s; var f;
		if (this.h < 10) { h = "0" + this.h; } else { h = this.h; }
		if (this.m < 10) { m = "0" + this.m; } else { m = this.m; }
		if (this.s < 10) { s = "0" + this.s; } else { s = this.s; }
		if (this.f < 10) { f = "0" + this.f; } else { f = this.f; }
		
		if(dropFrame)
		{
			document.getElementById('outpointBox').value = h + ";" + m + ";" + s + ";" + f;
		}
		else
		{
			document.getElementById('outpointBox').value = h + ":" + m + ":" + s + ":" + f;
		}
		// Format as 2 digit numbers
		var dh; var dm; var ds; var df;
		if (this.durH < 10) { dh = "0" + this.durH; } else { dh = this.durH; }
		if (this.durM < 10) { dm = "0" + this.durM; } else { dm = this.durM; }
		if (this.durS < 10) { ds = "0" + this.durS; } else { ds = this.durS; }
		if (this.durF < 10) { df = "0" + this.durF; } else { df = this.durF; }
	
		document.getElementById('durationBox').value = dh+":"+dm+":"+ds+":"+df;
		//enable the recorder selector
		document.getElementById('recorderSelector').disabled = false;

	};//end this.stopTake
	
}//end ingexLoggingTimecode


// --- Extending Ext library for Logging Module ---

// Ext.tree.ColumnTree = Ext.extend(Ext.tree.TreePanel, {
    // lines:false,
    // borderWidth: Ext.isBorderBox ? 0 : 2, // the combined left/right border for each cell
    // cls:'x-column-tree',
// 
    // onRender : function(){
        // Ext.tree.ColumnTree.superclass.onRender.apply(this, arguments);
        // this.headers = this.body.createChild(
            // {cls:'x-tree-headers'},this.innerCt.dom);
// 
        // var cols = this.columns, c;
        // var totalWidth = 0;
// 
        // for(var i = 0, len = cols.length; i < len; i++){
             // c = cols[i];
             // totalWidth += c.width;
             // this.headers.createChild({
                 // cls:'x-tree-hd ' + (c.cls?c.cls+'-hd':''),
                 // cn: {
                     // cls:'x-tree-hd-text',
                     // html: c.header
                 // },
                 // style:'width:'+(c.width-this.borderWidth)+'px;'
             // });
        // }
        // this.headers.createChild({cls:'x-clear'});
        // // prevent floats from wrapping when clipped
        // this.headers.setWidth(totalWidth);
        // this.innerCt.setWidth(totalWidth);
    // }
// });
// 
// Ext.tree.TakeNodeUI = Ext.extend(Ext.tree.TreeNodeUI, {
    // focus: Ext.emptyFn, // prevent odd scrolling behavior
// 
    // renderElements : function(n, a, targetNode, bulkRender){
        // this.indentMarkup = n.parentNode ? n.parentNode.ui.getChildIndent() : '';
// 
        // var t = n.getOwnerTree();
        // var cols = t.columns;
        // var bw = t.borderWidth;
        // var c = cols[0];
        // n.id = n.parentNode.attributes.databaseId +"-"+ n.attributes.databaseId;
        // n.attributes.id = n.id; 
// 
        // var buf = [
             // '<li><div ext:tree-node-id="',n.id,'" class="x-tree-node-el x-tree-node-leaf ', a.cls,'">',
                // '<div class="x-tree-col" style="width:',c.width-bw,'px;">',
                    // '<span class="x-tree-node-indent">',this.indentMarkup,"</span>",
                    // '<img src="', this.emptyIcon, '" class="x-tree-ec-icon x-tree-elbow">',
                    // //'<img src="', a.icon || this.emptyIcon, '" class="x-tree-node-icon',(a.icon ? " x-tree-node-inline-icon" : ""),(a.iconCls ? " "+a.iconCls : ""),'" unselectable="on">',
					// '<img src="/ingex/img/take.gif" unselectable="on" class="x-tree-node-icon" style="background-image:none;">',
                    // '<a hidefocus="on" class="x-tree-node-anchor" href="',a.href ? a.href : "#",'" tabIndex="1" ',
                    // a.hrefTarget ? ' target="'+a.hrefTarget+'"' : "", '>',
                    // '<span unselectable="on">', n.text || (c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),"</span></a>",
                // "</div>"];
         // for(var i = 1, len = cols.length; i < len; i++){
             // c = cols[i];
// 
             // buf.push('<div class="x-tree-col ',(c.cls?c.cls:''),'" style="width:',c.width-bw,'px;">',
                        // '<div class="x-tree-col-text" dataIndex="',c.dataIndex,'">',(c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),"</div>",
                      // "</div>");
         // }
         // buf.push(
            // '<div class="x-clear"></div></div>',
            // '<ul class="x-tree-node-ct" style="display:none;"></ul>',
            // "</li>");
// 
        // if(bulkRender !== true && n.nextSibling && n.nextSibling.ui.getEl()){
            // this.wrap = Ext.DomHelper.insertHtml("beforeBegin",
                                // n.nextSibling.ui.getEl(), buf.join(""));
        // }else{
            // this.wrap = Ext.DomHelper.insertHtml("beforeEnd", targetNode, buf.join(""));
        // }
// 
        // this.elNode = this.wrap.childNodes[0];
        // this.ctNode = this.wrap.childNodes[1];
        // var cs = this.elNode.firstChild.childNodes;
        // this.indentNode = cs[0];
        // this.ecNode = cs[1];
        // this.iconNode = cs[2];
        // this.anchor = cs[3];
        // this.textNode = cs[3].firstChild;
		// this.takeelements = new Object();
		// this.takeelements['location'] = this.elNode.childNodes[1].firstChild;
		// this.takeelements['date'] = this.elNode.childNodes[2].firstChild;
		// this.takeelements['inpoint'] = this.elNode.childNodes[3].firstChild;
		// this.takeelements['out'] = this.elNode.childNodes[4].firstChild;
		// this.takeelements['duration'] = this.elNode.childNodes[5].firstChild;
		// this.takeelements['result'] = this.elNode.childNodes[6].firstChild;
		// this.takeelements['comment'] = this.elNode.childNodes[7].firstChild;
    // },
// 
	// getTakeElement : function(name) {
		// if(typeof this.takeelements[name] != "undefined") return this.takeelements[name];
		// return false;
	// }
// });
// 
// Ext.tree.ItemNodeUI = Ext.extend(Ext.tree.TreeNodeUI, {
    // focus: Ext.emptyFn, // prevent odd scrolling behavior
// 
	// getItemNameEl : function(){
		// return this.itemNameNode;
	// },
// 	
	// getSequenceEl : function(){
		// return this.sequenceNode;
	// },
// 
    // renderElements : function(n, a, targetNode, bulkRender){
        // this.indentMarkup = n.parentNode ? n.parentNode.ui.getChildIndent() : '';
// 
        // var t = n.getOwnerTree();
        // var cols = t.columns;
        // var bw = t.borderWidth;
        // var c = cols[0];
        // n.id = n.attributes.databaseId;
        // n.attributes.id = n.id;
// 
        // var buf = [
                   // '<li><div ext:tree-node-id="',n.id,'" class="x-tree-node-el x-tree-node-leaf ', a.cls,'">',
                   // '<div class="x-tree-col" style="width:0px',c.width-bw,'px;">',
                   // '<span class="x-tree-node-indent">',this.indentMarkup,"</span>",
                   // '<img src="', this.emptyIcon, '" class="x-tree-ec-icon x-tree-elbow">',
                   // // '<img src="', a.icon || this.emptyIcon, '" class="x-tree-node-icon',(a.icon ? " x-tree-node-inline-icon" : ""),(a.iconCls ? " "+a.iconCls : ""),'" unselectable="on">',
                   // '<img src="/ingex/img/item.gif" unselectable="on">',
                   // '<a hidefocus="on" class="x-tree-node-anchor" href="',a.href ? a.href : "#",'" tabIndex="1" ',
                           // a.hrefTarget ? ' target="'+a.hrefTarget+'"' : "", '>',
                                   // '<span unselectable="on">',/* n.text || (c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),*/"</span></a>",
                                   // "</div>"];
// 
        // var seq = "[no sequence]";
        // if (a['sequence'] != "") seq = a['sequence'];
        // buf.push('<div class="x-tree-col" style="width:80px;border-bottom:1px solid #ddd;color: #449;">',
                // '<div class="x-tree-col-text-item" dataIndex="sequence">',seq,"</div>",
        // "</div>");
// 
        // buf.push('<div class="x-tree-col" style="width:800px;border-bottom:1px solid #ddd;color: #449;">',
                // '<div class="x-tree-col-text-item" dataIndex="itemName">',a['itemName'],"</div>",
        // "</div>");
// 
        // buf.push(
                // '<div class="x-clear"></div></div>',
                // '<ul class="x-tree-node-ct" style="display:none;"></ul>',
        // "</li>");
// 
        // if(bulkRender !== true && n.nextSibling && n.nextSibling.ui.getEl()){
            // this.wrap = Ext.DomHelper.insertHtml("beforeBegin",
                                // n.nextSibling.ui.getEl(), buf.join(""));
        // }else{
            // this.wrap = Ext.DomHelper.insertHtml("beforeEnd", targetNode, buf.join(""));
        // }
// 
        // this.elNode = this.wrap.childNodes[0];
        // this.ctNode = this.wrap.childNodes[1];
        // var cs = this.elNode.firstChild.childNodes;
        // this.indentNode = cs[0];
        // this.ecNode = cs[1];
        // this.iconNode = cs[2];
        // this.anchor = cs[3];
        // this.textNode = cs[3].firstChild;
		// this.itemNameNode = this.elNode.childNodes[1].firstChild;
		// this.sequenceNode = this.elNode.childNodes[2].firstChild;
    // }
// });

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
       
        tree.on('click', this.onNodeClick, this);
        //this.on('beforestartedit', this.fitToTree, this);
       
    },

    // private
    //fitToTree : function(ed, el){
     //   var td = this.tree.getTreeEl().dom, nd = el.dom;
      //  if(td.scrollLeft >  nd.offsetLeft){ // ensure the node left point is visible
       //     td.scrollLeft = nd.offsetLeft;
        //}
       // var w = Math.min(
        //        this.maxWidth,
         //       (td.clientWidth > 20 ? td.clientWidth : td.offsetWidth) - Math.max(0, nd.offsetLeft-td.scrollLeft) - /*cushion*/5);
        //this.setSize(w, '');
    //},
  
    
    
    //private
    onNodeClick : function(node, e){
       
        //node.select();
        ILcurrentItemId = node.id;
        
        var nodeDepth = node.getDepth();
        if (nodeDepth == 1) //if is an item expand
        {
            ILexpandSingleItem();
        }
    }//end onNodeClick

});
