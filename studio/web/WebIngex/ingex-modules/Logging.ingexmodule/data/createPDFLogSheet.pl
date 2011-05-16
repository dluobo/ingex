#!/usr/bin/perl -wT

# Copyright (C) 2011  British Broadcasting Corporation
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

##########################################################################################
#
# createPDFLogSheet script - creates a pdf log sheet for editing purposes from supplied programme 
#
##########################################################################################

use warnings;
use strict;

use lib ".";
use lib "..";
use lib "../../../ingex-config";
use File::Temp qw(mktemp tempfile);
use JSON::XS;
use CGI ':standard';
use PDF::Create;
use ingexconfig;
use prodautodb;

# unix tmp directory
my $tempdir = '/tmp';

print header;

# ingex db
my $dbh = prodautodb::connect(
		$ingexConfig{"db_host"}, 
		$ingexConfig{"db_name"},
		$ingexConfig{"db_user"}, 
		$ingexConfig{"db_password"})
 	or die("DB Unavailable");


#settings for the pdf and global variables
my $a4; # holds a4 page properties
my $pageNum = 0; # holds the page number of current page
# current page we are working on, use to write to page
my $page;
 
#title strings
my $series = "";
my $programme = "";

#edit rate - TODO hard coded here, needs to be received along with take, item or programme info
my $editRate = 25;

# array for the items
my @items;
my $numOfItems = 0;
my $currentItemIndex = 0;

# array for the takes
my @takes;
my $numOfTakes = 0;
my $currentTakeIndex = 0;

# co-ordinates start in bottom left hand corner of portrait page
# These are the bounds used to allow for hole punching and/or stapling
# defines the writable area
my $horizLeft = 50;
my $printableAreaBottom = 50;
my $horizRight = 550;
my $printableAreaTop = 800;

my $pageWidth = $horizRight - $horizLeft;
my $pageHeight = $printableAreaTop - $printableAreaBottom;

# define the centre of the writable area
my $horizCentre = (($horizRight-$horizLeft)/2);
my $vertCentre = (($printableAreaTop-$printableAreaBottom)/2);

# define the area for the header - titles written here
my $headerTop = $printableAreaTop;
my $headerBottom = $printableAreaTop - 50;

# defines the vertical centre of header
my $headerVertCentre = $headerTop - (($headerTop-$headerBottom)/2);

# define the area for the footer - titles written here
my $footerTop = $printableAreaBottom + 50;
my $footerBottom = $printableAreaBottom;

# defines the vertical centre of footer - page numbers written here
my $footerVertCentre = $footerTop - (($footerTop-$footerBottom)/2);

# define the main body writable area
my $bodyTop = $headerBottom;
my $bodyBottom = $footerTop;

# define the main body size
my $bodyWidth = $horizRight - $horizLeft;
my $bodyHeight = $headerBottom - $footerTop;

# define the vertical centre of main body area
my $bodyVertCentre = $bodyTop - (($bodyTop-$bodyBottom)/2);

#declaration of font variables set later in write_pdf
my $font_des;
my $font_title;

# font sizes for the various elements
# TODO may need separating into individual parts for the takes data
my $titleFontSize = 20;
my $columnTitleFontSize = 14;
my $itemFontSize = 16;
my $takeFontSize = 12;
my $pageNumFontSize = 12;

#holds the factor by which the fontsize is modified to give page 'pixels' vertically
#Average may be closer to 0.7 of fontsize but use 0.9 to be safe
my $itemFontHeightFactor = 0.9;
my $takeFontHeightFactor = 0.9;

# Horizontal bounding line vertical separation from text
# e.g. draw posn will move down by this amount and line drawn at that posn
my $boundingLineVertSeparation = 10;

#sets the vertical separation of the timecode elements, line spacing
my $tcVertLineSeparation = 5;

#the distance that is added before a new item is drawn
my $itemVertSeparation = 20;

# item Name offset - the amount in from the left margin that the item name is drawn at
my $itemNameHorizOffset = 10;

# will be used to draw column bounding lines in the table, ensure are definitely initialised
my $currentItemTop = $bodyTop;
my $currentItemBottom = $bodyBottom;
my $currentTakeTop = $bodyTop;
my $currentTakeBottom = $bodyBottom;

# defines the current posn vertically that will be used to draw elements
my $currentVertDrawPosn = $bodyTop;

# the current space available/remaining on the page
my $currentVertSpaceAvailable = $currentVertDrawPosn - $bodyBottom;

#set the space available for comment and item title - comment space is altered in dynamic column spacing
my $commentSpace = $pageWidth;
my $itemSpace = $pageWidth - $itemNameHorizOffset;

# Array to hold the titles, proportions and limits of columns
# Limits are dynamically set later by the number of characters expected
# associative array %columnIndex holds a more readable indexing system for use in printing
# if specific sizes are required for the columns these can be set below in the array
# If doing this need to comment out or remove the dynamic size assignment section in write_pdf
my @tableColumns = (
            { Title => "Dialogue",        #index 0 in associative array
              TextMaximum => "999_-_999", #allows for "999 - 999"
              LeftLimit => 0,             #assigned programatically below from left limit of writable area
              RightLimit => 0,            #assigned programatically below using spae for max text and the offset
              Width => 0,                 #assigned programatically below using left and right limit
              Centre => 0,                #assigned programatically below using width
              TextOffset => 3,            #this is the page 'pixel' offset that text will start at if left justified
                                          #if centrally justified this gives the space at left and right hand side of text
            },
            { Title => "Takes",            #index 1
              TextMaximum => "-999-",       #allows sufficient space for "999" and bounding box
              LeftLimit => 0,
              RightLimit => 0,
              Width => 0,
              Centre => 0,
              TextOffset => 3,
            },
            { Title => "Timecode",        #index 2
              TextMaximum => "date-_2000-20-30",  #allows for "date: YYYY-MM-DD" and "out : HH:MM:SS:FF"
              LeftLimit => 0,
              RightLimit => 0,
              Width => 0,
              Centre => 0,
              TextOffset => 10,
            },
            { Title => "Dur",             #index 3
              TextMaximum => "999-_99-",            #allows for (MMM" SS')
              LeftLimit => 0,
              RightLimit => 0,
              Width => 0,
              Centre => 0,
              TextOffset => 3,
            },
            { Title => "Comments",        #index 4
              LeftLimit => 0,
              RightLimit => 0,
              Width => 0,
              Centre => 0,
              TextOffset => 10,
            },
);

# content => which column drawn into from associative array data structure
# access $tableColumns[$columnIndex{'DiaRange'}] then will get $tableColumns[0] from above array
# this is to allow for more readable indexing in the code of column elements
my %columnIndex = (
            'DiaRange',  0,
            'Takes',     1,
            'TC' ,       2,
            'Duration' , 3,
            'Comments' , 4,
);

#forward declarations of sub-routines to avoid warnings
sub validate_params;
sub write_pdf_log;

my $errorMessage;
#check that data has been passed to script
if (($errorMessage = validate_params()) eq "ok")
{
    my $ok = "yes";
  
    # get parameters from json and store in a perl data structure
    my $jsonStr = param('jsonIn');
  
    #creates a hash containing elements from the json string
    my $jsonData = decode_json($jsonStr);
  
    # generate the pdf
    my $filename = write_pdf_log($jsonData);
    # TODO need some method of determining success or failure of PDF creation
    
    #create filename hash
    my %results = ("filename" => $filename);

    #encode filename as json to be returned to browser
    my $filenameJson = encode_json( \%results );

    # TODO change to only return filename on success
    print ' '.$filenameJson.' ';	
}

#disconnect from database and exit
prodautodb::disconnect($dbh) if ($dbh);
exit (0);

#checks whether any data has been passed as a parameter to the perl script
sub validate_params()
{
    return "No input data defined" if (!defined param('jsonIn') || param('jsonIn') =~ /^\s*$/);
    return "ok";
}

# declare write_pdf
sub write_pdf_log (@)
{
    #get passed data into private hash
    my ($data) = @_;
    
    #get the titles of pages
    $series = $data->{'Series'};
    $programme = $data->{'Programme'};
        
    # DataRoot points to an array as value (reference) for the key value pairs so dereference the array to get the data in a usable array form
    @items = @{$data->{'DataRoot'}};
    $numOfItems = scalar(@items);
    
    # TODO make filename creation more like that of createaaf so a bit more self evident as to what the file contains
    # This could also do with having a time based name along with Series/Prog name to avoid conflicting filenames
    #get a file handle and filename
    my ($fh, $filename ) = tempfile( DIR => $tempdir, SUFFIX => '.pdf' );   # create unique temp file
    
    #initialise pdf
    my $pdf = new PDF::Create(              'filename'     => $filename,
                                            'Author'       => 'Ingex', 
                                            'Title'        => $programme, 
                                            'CreationDate' => [ localtime ], );


# Comment/uncomment out here to change all fonts

# Prepare fonts that will be used
#     $font_des = $pdf->font('BaseFont' => 'Helvetica');
#     $font_title = $pdf->font('BaseFont' => 'Helvetica-Bold');
   
#     $font_des = $pdf->font('BaseFont' => 'Courier');
#     $font_title = $pdf->font('BaseFont' => 'Courier-Bold');
   
   $font_des = $pdf->font('BaseFont' => 'Times-Roman');
   $font_title = $pdf->font('BaseFont' => 'Times-Bold');

    # add an A4 sized page object
    $a4 = $pdf->new_page('MediaBox' => $pdf->get_page_size('A4'));

    my $textWidth = 0;            #width of maximum sized text entry for a column
    my $columnTitleTextWidth = 0; #width of column title text

    my $tableIndex = 0;
    foreach (@tableColumns)
    {
       if (0 == $tableIndex)
       {
            $tableColumns[0]->{'LeftLimit'} = $horizLeft;
       }
       else
       {
            $tableColumns[$tableIndex]->{'LeftLimit'} = $tableColumns[($tableIndex -1)]->{'RightLimit'};
       }
      
       if ("Comments" eq $tableColumns[$tableIndex]->{'Title'})
       {
          $tableColumns[$tableIndex]->{'RightLimit'} = $horizRight;
       }
       else
       {
           # get the size of this group of letters/ bit of text using PDF::Create
           # +1 is used to ensure that all fractional numbers are raised to the nearest integer higher than current value
           $textWidth = int(($a4->string_width($font_des, $tableColumns[$tableIndex]->{'TextMaximum'}) * $takeFontSize) 
                         + $tableColumns[$tableIndex]->{'TextOffset'} + 1);

           $columnTitleTextWidth = int(($a4->string_width($font_des, $tableColumns[$tableIndex]->{'Title'}) * $columnTitleFontSize) 
                                    + $tableColumns[$tableIndex]->{'TextOffset'} + 1);

          # find which of the column title or maximum length column text is larger and then use that to set the column limits, width and centre
           if ($textWidth > $columnTitleTextWidth)
           {
              $tableColumns[$tableIndex]->{'RightLimit'} = ($tableColumns[$tableIndex]->{'LeftLimit'} + $textWidth);
           }
           else
           {
              $tableColumns[$tableIndex]->{'RightLimit'} = ($tableColumns[$tableIndex]->{'LeftLimit'} + $columnTitleTextWidth);
           }
       }
    
       # calculate width and centre point
       # int function just throws away the fractional part, doesn't round the value
       $tableColumns[$tableIndex]->{'Width'} = ($tableColumns[$tableIndex]->{'RightLimit'} - $tableColumns[$tableIndex]->{'LeftLimit'});
       $tableColumns[$tableIndex]->{'Centre'} = $tableColumns[$tableIndex]->{'LeftLimit'} + int( ($tableColumns[$tableIndex]->{'Width'} / 2) + 0.5);
      
       $tableIndex++;
      
    }# end of column for loop
        
    # define the available comment column space
    $commentSpace = $horizRight 
                    - $tableColumns[(scalar(@tableColumns)-1)]->{'LeftLimit'} #gives overall width of column from left line to right line
                    - $tableColumns[(scalar(@tableColumns)-1)]->{'TextOffset'} #gives width of writeable area from beginning of text to right column line
                    - 5; #ensure doesn't draw right up to right hand line
 
   # Add a real page which inherits its attributes from $a4
   new_page();

   # If wish to draw header and footer bounding box uncomment these
   # draw_header_box();
   # draw_footer_box();
   
    # holds the currentItem
    $currentItemIndex = 0;
   
    # add the items
    foreach (@items)
    {
      add_item();
      $currentVertDrawPosn = $currentVertDrawPosn - $itemVertSeparation; #move to the position of the next item 
      $currentItemIndex++;
    }
    
    # Close the file and write the PDF
    $pdf->close;
    return $filename;
    
}#end sub write_pdf

sub new_page
{
    #increment the page number
    $pageNum++;
    #move page indicator to the new page and create it
    $page = $a4->new_page;
    #draw the page number on the new page
    add_page_num();
    #add titles to this new page
    add_titles();

    #reset the various item, take and draw positions for the new page
    $currentItemTop = $bodyTop;
    $currentTakeTop = $bodyTop;
    $currentTakeBottom = $bodyBottom;
    $currentVertDrawPosn = $bodyTop;

    #add the titles for the columns and their bounding boxes
    add_column_titles();

    #leave a gap before starting the item printing on a new page
    $currentVertDrawPosn = $currentVertDrawPosn - $itemVertSeparation;
}

# draws the current page number in the footer
sub add_page_num
{
    #put page number in the footer of each page
    $page->stringc($font_des, $pageNumFontSize, $horizCentre, $footerVertCentre, $pageNum);
}

# draws a bounding box round the header area 
sub draw_header_box
{
    #header box
    $page->line($horizLeft, $headerBottom, $horizRight, $headerBottom);
    $page->line($horizLeft, $headerTop, $horizRight, $headerTop);
    $page->line($horizLeft, $headerBottom, $horizLeft, $headerTop);
    $page->line($horizRight, $headerBottom, $horizRight, $headerTop);
}

# draws a bounding box round the footer area 
sub draw_footer_box
{
    #footer Box
    $page->line($horizLeft, $footerBottom, $horizRight, $footerBottom);
    $page->line($horizLeft, $footerTop, $horizRight, $footerTop);
    $page->line($horizLeft, $footerBottom, $horizLeft, $footerTop);
    $page->line($horizRight, $footerBottom, $horizRight, $footerTop);
}

# Take the preset titles of series and programmes and print them in Header box
sub add_titles
{
    my @wrappedSeries = wrap_logger_text($series, $font_title, $titleFontSize, $pageWidth);
    my $numOfSeriesLines = scalar(@wrappedSeries);

    $wrappedSeries[0] = "Series : ".$wrappedSeries[0];
    my $titleVertDrawPosn = ($headerTop - $titleFontSize);

    foreach(@wrappedSeries)
    {
        $page->string($font_title, $titleFontSize, $horizLeft, $titleVertDrawPosn, $_);
        $titleVertDrawPosn = $titleVertDrawPosn - $titleFontSize; 
    }

    my @wrappedProgramme = wrap_logger_text($programme, $font_title, $titleFontSize, $pageWidth);
    my $numOfProgrammeLines = scalar(@wrappedProgramme);

    $wrappedProgramme[0] = "Programme : ".$wrappedProgramme[0];

    foreach(@wrappedProgramme)
    {
        $page->string($font_title, $titleFontSize, $horizLeft, $titleVertDrawPosn, $_);
        $titleVertDrawPosn = $titleVertDrawPosn - $titleFontSize;
    }

    $headerBottom = $titleVertDrawPosn;
    $bodyTop = $titleVertDrawPosn;
}

# draws the titles of the columns on a page
sub add_column_titles
{
    # set the top and bottom limits for column line drawing
    my $colTitleTop = $currentVertDrawPosn;

   # move to posn of colTitle writing
    $currentVertDrawPosn = $bodyTop - $columnTitleFontSize;

    # Print the column title text
    $page->stringc($font_des, $columnTitleFontSize, $tableColumns[$columnIndex{'DiaRange'}]->{'Centre'}, $currentVertDrawPosn, "$tableColumns[$columnIndex{'DiaRange'}]->{'Title'}");
    $page->stringc($font_des, $columnTitleFontSize, $tableColumns[$columnIndex{'Takes'}]->{'Centre'}, $currentVertDrawPosn, "$tableColumns[$columnIndex{'Takes'}]->{'Title'}");
    $page->stringc($font_des, $columnTitleFontSize, $tableColumns[$columnIndex{'TC'}]->{'Centre'}, $currentVertDrawPosn, "$tableColumns[$columnIndex{'TC'}]->{'Title'}");
    $page->stringc($font_des, $columnTitleFontSize, $tableColumns[$columnIndex{'Duration'}]->{'Centre'}, $currentVertDrawPosn, "$tableColumns[$columnIndex{'Duration'}]->{'Title'}");
    $page->stringc($font_des, $columnTitleFontSize, $tableColumns[$columnIndex{'Comments'}]->{'Centre'}, $currentVertDrawPosn, "$tableColumns[$columnIndex{'Comments'}]->{'Title'}");
    
    #move to posn of lower horiz bounding line 
    $currentVertDrawPosn = $currentVertDrawPosn - $boundingLineVertSeparation;

    #draw the bounding lines for the columns passing position of top and bottom of column titles
    draw_column_boxes($colTitleTop, $currentVertDrawPosn);
}

#draws the column box bounding lines
sub draw_column_boxes(@)
{
    #get the passed limits - (top Posn, Bottom Posn)
    my (@colLimits) = @_;
    
    my $leftLine = 0;
    my ($tableIndex) = 0;
    foreach (@tableColumns)
    {
        #draw the left hand limit of column
        #use top and bottom coords of current Item title to draw the lines for the columns in between item names
        $leftLine = $tableColumns[$tableIndex]->{'LeftLimit'};
        $page->line($leftLine, $colLimits[0], $leftLine, $colLimits[1]);
        $tableIndex++;
    }
    
    #close off columns with final right limit line
    $page->line($horizRight, $colLimits[0], $horizRight, $colLimits[1]);
    
    #draw horizontal base bounding line
    $page->line($horizLeft, $colLimits[0], $horizRight, $colLimits[0]);
    $page->line($horizLeft, $colLimits[1], $horizRight, $colLimits[1]);
    
}

#returns a word wrapped version of the text in an array, 1 line per element
sub wrap_logger_text
{
   #get passed input values
   my ($inputText, $fontUsed, $textFontSize, $maxAvailableSpace) = @_;
   
   #split text into separate words based on any consecutive whitespace separators space, newline, tab etc.
   my @wordArray = split(/\s+/, $inputText);
   my $numOfWords = scalar(@wordArray);
   
   my @paragraph;
   my $spaceLeftOnLine = $maxAvailableSpace;
   my $currentLine = 0;
   my $totalLineLength = 0;
   my $sizeOfSpace = ( $page->string_width($fontUsed, " ") )  * $textFontSize;
    
   # Check that word isn't longer than space available, returns the space occupied by word if fits on line, otherwise returns zero
   my $wordCheck = check_word_fits($wordArray[0], $fontUsed, $textFontSize, $maxAvailableSpace);

   # for all words following the first word
   for(my $currentWord = 0; $currentWord < scalar(@wordArray); $currentWord++)
   {
       $inputText = $wordArray[$currentWord];
       #check this word fits into the whole line space
       $wordCheck = check_word_fits($inputText, $fontUsed, $textFontSize, $maxAvailableSpace);

       if($wordCheck)# if word does fit on space provided by one line
       {
           #check if there is sufficient space left on this line to add this word
           if(($totalLineLength + $wordCheck + $sizeOfSpace) < $maxAvailableSpace)
           {
                #add word to the line
                if ($currentWord < 1) #if first word
                {
                    $paragraph[$currentLine] = "$wordArray[$currentWord]";
                }
                else #if any word following first word
                {
                    $paragraph[$currentLine] .= " $wordArray[$currentWord]";
                }
                $totalLineLength = $totalLineLength + $wordCheck + $sizeOfSpace;
                $spaceLeftOnLine = $spaceLeftOnLine - $wordCheck - $sizeOfSpace;
           }
           else     # word does not fit on line in remaining space, but does fit in the space provided by a whole line
           {
                # go to next line
                $currentLine++;
                #reset the space left and used
                $spaceLeftOnLine = $maxAvailableSpace - $wordCheck;
                $totalLineLength = $wordCheck;
                $paragraph[$currentLine] = $wordArray[$currentWord];
           }

       } # end if word does fit in one whole line space
       else # Word doesn't fit in space of a whole single line
       {
            # TODO make this cope better with wrapping, currently works on average character width and guesses for word rather than adding up characters and testing width each time
            # so words like iiiiiiiiiiiiiiHHHHHHHH would probably leave a gap on wrap as guess would be significantly less than space available - needs testing

            my $inputTextWidth = $page->string_width($fontUsed, $wordArray[$currentWord]);
            # get width in 'page pixels' for the given fontsize
            my $wordLength =  $inputTextWidth * $textFontSize;
            # get the ratio of space available to space used by word
            my $spaceWordRatio = $maxAvailableSpace/$wordLength;
            # get num of characters in long word that are trying to wrap
            my $numOfCharInWord = length($wordArray[$currentWord]);
            # get number of characters in suggested word that may fit - could be above or below the space available when calculated
            # guess test word using the number of characters that fit in the space available using average width
            # if space available is 100 and word length is 150 guess that 2/3 of characters of long word will take up approx the space available
            # actual word could be above or below the space available if non-monospace font used
            my $testWordLen = int($spaceWordRatio * $numOfCharInWord);
            # assign substr of this number of characters to test word and the rest to a remainder word
            my $testWord = substr($wordArray[$currentWord], 0, $testWordLen);
            my $remainderWord = substr($wordArray[$currentWord],$testWordLen);
            #add continuation charac to the test word and a beginning space
            $testWord = " $testWord-";
            my $characterIndex = 0;
            my $testWordCheck = 0;
    
            while($testWordCheck < 1)
            {
                # perform a check to see if test word fits in space available
                $testWordCheck = check_word_fits($testWord, $fontUsed, $textFontSize, $spaceLeftOnLine);
    
                if ($testWordCheck > 0) # if test word does fit in the space available
                {
                    
                    $characterIndex = length($testWord)-2;
                    if($spaceLeftOnLine < $maxAvailableSpace) #if there are words already on this line
                    {
                        $paragraph[$currentLine] .= $testWord; # add onto this line
                    }
                    else #if is a totally new line
                    {
                        $paragraph[$currentLine] = substr($testWord,1); # take off leading space if is on a new line and add to this line
                    }

                    # go to new line as current line will be full
                    $currentLine++;
                    # reset space available to that of whole line as on new line
                    $spaceLeftOnLine = $maxAvailableSpace;
                    # set test word = remainder word with space and continuation character
                    $testWordLen = length($remainderWord);
                    $testWord = " $remainderWord-";
                    
                    #if remainder doesn't fit then will continue looping
                    #if remainder does fit then it will exit loop - this means final bit of word not written in while loop, must be written afterwards
                    $testWordCheck = check_word_fits($testWord, $fontUsed, $textFontSize, $spaceLeftOnLine);
                }
                else # test word doesn't fit in space available
                {
                    #take a character off and re-test continually until it does fit
                    $testWordLen = $testWordLen - 1;
                    # assign substr of this new test length to test word and the rest to a remainder word
                    $testWord = substr($wordArray[$currentWord], $characterIndex , $testWordLen);
                    $remainderWord = substr($wordArray[$currentWord],($characterIndex+$testWordLen));
                    #add continuation charac to the test word and beginning space
                    $testWord = " $testWord-";
                } #end if test word doesn't fit in space available
    
            } # end while the word doesn't fit on line
    
            #remove continuation character and beginning space from word before adding to line as is at start of a new line
            $paragraph[$currentLine] = substr($testWord, 1, (length($testWord)-2));
            #set the new space available and space used
            $totalLineLength = $testWordCheck;
            $spaceLeftOnLine = $maxAvailableSpace - $testWordCheck;
       } # end word doesnt fit on a single line

   }#end for the words loop

   return @paragraph;
}#end wrap_logger_text

sub check_word_fits
{
    my ($inputText, $inputFontUsed, $inputFontSize, $remainingSpace) = @_;
    # get width in page default units for provided text and font used
    my $inputTextWidth = $page->string_width($inputFontUsed, $inputText);
    # get width in 'page pixels' for the given fontsize
    my $inputTextWidthForFont = $inputTextWidth * $inputFontSize;

    if ($inputTextWidthForFont > $remainingSpace)
    {
        return 0;
    }
    else
    {
        return $inputTextWidthForFont;
    }

}

#draws the item data and calls the takes sub-routine to draw takes data
sub add_item
{
    #need to work out if this is going to go over the page - this has to include the minimum take size of three lines of takeFontSize and spacing
    my $itemName = $items[$currentItemIndex]->{'itemName'};
    my @wrappedItem = wrap_logger_text($itemName, $font_des, $itemFontSize, $itemSpace);
    my $numOfItemLines = scalar(@wrappedItem);

    # TODO - is this needed? only used if drawing item bounding box verticals 
    $currentItemTop = $currentVertDrawPosn;
    
    #move to posn of base of first line of item Title
    $currentVertDrawPosn = $currentVertDrawPosn - $itemFontSize;
    
    #holds the space required for the next entry
    my $itemSpaceRequired = $numOfItemLines * $itemFontSize;
    my $takeSpaceRequired = 0;
 
    #for the item get the takes
    @takes = @{$items[$currentItemIndex]->{'children'}};
    $numOfTakes = scalar(@takes);

    # in case that there are no Takes   
    if ($numOfTakes == 0)
    {
       #check that there is enough space for the item title and the takes 
       $currentVertSpaceAvailable = $currentVertDrawPosn - $bodyBottom;
       $takeSpaceRequired = $takeFontSize;
       my $requiredSpace = ($takeSpaceRequired + $itemSpaceRequired);
      
       #if there is not enough space available
       if ($currentVertSpaceAvailable < $requiredSpace)
       {
           #add new page
           new_page();
           
           $currentVertDrawPosn = $currentVertDrawPosn - $itemFontSize;
            
            foreach(@wrappedItem)
            {
                $page->string($font_des, $itemFontSize, ($horizLeft+$itemNameHorizOffset), $currentVertDrawPosn, $_);
                $currentVertDrawPosn = $currentVertDrawPosn - $itemFontSize; 
            }
            #move back up to base of item title
            $currentVertDrawPosn = $currentVertDrawPosn + $itemFontSize; 
     
           #move below text for  horizontal bounding line
           $currentVertDrawPosn = $currentVertDrawPosn - $boundingLineVertSeparation;
           $page->line($horizLeft, $currentVertDrawPosn, $horizRight, $currentVertDrawPosn);
                               
           # need to move to take position in order to start writing
           $currentVertDrawPosn = $currentVertDrawPosn - $takeFontSize;
                      
       } # end if going to new page        
       else
       {
            # NOT Going to a new page with no takes for this item
           
           #print item title
           foreach(@wrappedItem)
           {
              $page->string($font_des, $itemFontSize, ($horizLeft+$itemNameHorizOffset), $currentVertDrawPosn, $_);  
              $currentVertDrawPosn = $currentVertDrawPosn - $itemFontSize; 
           }
              
           #move back up to base of item title
           $currentVertDrawPosn = $currentVertDrawPosn + $itemFontSize; 
                  
           #move below text for  horizontal bounding line
           $currentVertDrawPosn = $currentVertDrawPosn - $boundingLineVertSeparation;
           $page->line($horizLeft, $currentVertDrawPosn, $horizRight, $currentVertDrawPosn);
           
           # need to move to take position in order to start writing
           $currentVertDrawPosn = $currentVertDrawPosn - $takeFontSize;
           
       } #end if not going to new page
       
        #print no takes for item in columns
        $page->stringc($font_des, $takeFontSize, $tableColumns[$columnIndex{'Takes'}]->{'Centre'}, $currentVertDrawPosn, "N/A");
        $page->stringc($font_des, $takeFontSize, $tableColumns[$columnIndex{'Comments'}]->{'Centre'}, $currentVertDrawPosn, "No Takes");  
        #move to position of bounding line
        $currentVertDrawPosn = $currentVertDrawPosn - $boundingLineVertSeparation;
        #draw horizontal bounding line
        $page->line($horizLeft, $currentVertDrawPosn, $horizRight, $currentVertDrawPosn);
           
    } #end if there are no takes 
    else
    {     # if there ARE takes

        # Add the takes
        $currentTakeIndex = 0;
        foreach (@takes)
        {
           #calculate the space required for this takes comments
           $currentVertSpaceAvailable = $currentVertDrawPosn - $bodyBottom;
           my $comment = $takes[$currentTakeIndex]->{'comment'};
           my @wrappedComment = wrap_logger_text($comment, $font_des, $takeFontSize, $commentSpace);
           my $numOfCommentLines = scalar(@wrappedComment);
           my $spaceReqForComment = $numOfCommentLines * $takeFontHeightFactor * 2;
           # calculate the space taken by the timecode info
           my $spaceReqForTc = ($takeFontSize * $takeFontHeightFactor * 3) +($tcVertLineSeparation * 2);
           # calcualte the space required by the sequence or dia range
           my @sequence = @{$items[$currentItemIndex]->{'sequence'}};
           # first element
           my $seqStart = $sequence[0];
           # last element
           my $seqEnd = $sequence[$#sequence];
           my $seqRange = "$seqStart - $seqEnd";
           my @wrappedSeqRange = wrap_logger_text($seqRange, $font_des, $takeFontSize, ($tableColumns[$columnIndex{'DiaRange'}]->{'Width'} - 10));
           my $numOfSeqLines = scalar(@wrappedSeqRange);
           my $spaceReqForDiaRange = $numOfSeqLines * $takeFontHeightFactor * 2;

           $takeSpaceRequired = $spaceReqForTc;
           if ($spaceReqForComment > $takeSpaceRequired)
           {
                $takeSpaceRequired = $spaceReqForComment;
           }

           if ($spaceReqForDiaRange > $takeSpaceRequired)
           {
                $takeSpaceRequired = $spaceReqForDiaRange;
           }

           my $requiredSpace = ($takeSpaceRequired + $itemSpaceRequired);
      
           # if there is not enough space available
           if ($currentVertSpaceAvailable < $requiredSpace)
           {
               new_page();

               $currentVertDrawPosn = $currentVertDrawPosn - $itemFontSize;
             
              foreach(@wrappedItem)
              {
                  $page->string($font_des, $itemFontSize, ($horizLeft+$itemNameHorizOffset), $currentVertDrawPosn, $_); 
                  $currentVertDrawPosn = $currentVertDrawPosn - $itemFontSize; 
              }
              
                #move back up to base of item title
                $currentVertDrawPosn = $currentVertDrawPosn + $itemFontSize; 
                         
               
              #move below text for  horizontal bounding line
              $currentVertDrawPosn = $currentVertDrawPosn - $boundingLineVertSeparation;
              $page->line($horizLeft, $currentVertDrawPosn, $horizRight, $currentVertDrawPosn);
                         
           } # if going to new page
           else
           {
                #if is the first take then add the item title
                if ($currentTakeIndex < 1)
                {
                    
                    foreach(@wrappedItem)
                    {
                       $page->string($font_des, $itemFontSize, ($horizLeft+$itemNameHorizOffset), $currentVertDrawPosn, $_); 
                       $currentVertDrawPosn = $currentVertDrawPosn - $itemFontSize; 
                    }
               
                    #move back up to base of item title
                    $currentVertDrawPosn = $currentVertDrawPosn + $itemFontSize; 
                    #move below text for  horizontal bounding line
                    $currentVertDrawPosn = $currentVertDrawPosn - $boundingLineVertSeparation;
                    $page->line($horizLeft, $currentVertDrawPosn, $horizRight, $currentVertDrawPosn);
                
                }#end if first take for item
           
           }# end if not a new page
           
           # set position of top of take so can draw vertical line to here
           $currentTakeTop = $currentVertDrawPosn;
           # need to move to take position in order to start writing on a new page
           $currentVertDrawPosn =  $currentVertDrawPosn - $takeFontSize;
           #add the information for the take
           add_take(\@wrappedComment, \@wrappedSeqRange);
           $currentTakeBottom = $currentVertDrawPosn;
           # actually draw the columns
           draw_column_boxes($currentTakeTop, $currentTakeBottom); #draw column boundaries for current Take
           $currentTakeIndex++;
        
        } #end for each take
    } #end if there are takes
}#  end of sub add_item()

#draws to takes data
sub add_take
{
    # Get passed array of comment lines (pre-wrapped comments) and seq or dialogue range
    my (@comments) = @{ $_[0]};
    my (@seqRange) = @{ $_[1]};

    # Get the other information for this take
    my $result = $takes[$currentTakeIndex]->{'result'};
    my $takeNum = $takes[$currentTakeIndex]->{'takeNo'};
    my $inpoint = $takes[$currentTakeIndex]->{'inpoint'};
    my $out = $takes[$currentTakeIndex]->{'out'};
    my $date = $takes[$currentTakeIndex]->{'date'};

    my $seqRangeVertDrawPosn = $currentVertDrawPosn;
    my $seqRangeSpaceUsed = 0;

    foreach(@seqRange)
    {
        $page->stringc($font_des, $takeFontSize, $tableColumns[$columnIndex{'DiaRange'}]->{'Centre'}, $seqRangeVertDrawPosn, $_);
        $seqRangeVertDrawPosn = $seqRangeVertDrawPosn - ($takeFontSize * $takeFontHeightFactor);
        $seqRangeSpaceUsed = $seqRangeSpaceUsed + ($takeFontSize * $takeFontHeightFactor);
    }

#### DRAW TAKENUM
    # check if take is a marked take or not and then print take number with a bounding box accordingly
    # TODO change this so that it compares to the text from the database matched to takeresult id=2
    # or change rest of system and load_takes in prodautodb to return result id rather than result text for a take
    if ($result eq "Circled")
    {
        # get the posn of base of text
        my $takeNumVertPosn = $currentVertDrawPosn - $takeFontSize;
        # the offset from vertical Posn for drawing horizontal lines
        my $topPosn =($takeNumVertPosn+($takeFontSize));
        my $basePosn = ($takeNumVertPosn - 5);
        
        # need to know how many characters there are in take num to work out properly
        my $takeNumWidth = $page->string_width($font_des, $takeNum);
        my $leftPosn = $tableColumns[$columnIndex{'Takes'}]->{'Centre'} 
                       - int(($takeNumWidth / 2) + 0.5) 
                       - 10;
        my $rightPosn = $tableColumns[$columnIndex{'Takes'}]->{'Centre'} 
                        + int(($takeNumWidth / 2) + 0.5) 
                        + 10;
        # base line
        $page->line( $leftPosn, $basePosn,
                     $rightPosn , $basePosn);
                          
        # top line
        $page->line( $leftPosn, $topPosn,
                     $rightPosn , $topPosn);
                      
        # left line
        $page->line( $leftPosn, $topPosn,
                     $leftPosn , $basePosn);
                        
        # right line
        $page->line( $rightPosn, $topPosn,
                     $rightPosn , $basePosn);      
       # use font_title so have bold version of font or looks different if a diff font for a marked take
       $page->stringc($font_title, $takeFontSize, $tableColumns[$columnIndex{'Takes'}]->{'Centre'}, $takeNumVertPosn, $takeNum);

    } #end of if take result is marked
    else
    {
       $page->stringc($font_des, $takeFontSize, $tableColumns[$columnIndex{'Takes'}]->{'Centre'}, ($currentVertDrawPosn - $takeFontSize), $takeNum);
    }

##### DRAW DURATION
    # convert duration to minutes and seconds by splitting string based on : and then change to 56" 5' format
    # TODO then round the frames?? for an accurate rounding ability then need to know the take editrate
    # Need to add editrate to what the getProgramme.pl script returns if wish to calculate in this manner
    # TODO will need to add support for dropframe ; in future a flag will be required for this
     my $duration = $takes[$currentTakeIndex]->{'duration'};
                            #hh:mm:ss:ff gives 4 array elements
    my @timeCodeSplit = split(/:/, $duration);
    my $convertedDurationMin = $timeCodeSplit[0]*60 + $timeCodeSplit[1];
    # TODO do a proper rounding based on mod of frames and framerate or just > or < 12.5 etc or other
    # At the moment just add one second on so duration definitely encapsulates the whole take
    my $convertedDurationSec = $timeCodeSplit[2] + 1;
    
    # TODO is case where take lasts longer than 1 day need to be handled
    
# handle the case where the addition of a second completes the seconds    
    if ($convertedDurationSec > 59)
    {
        $convertedDurationMin = $convertedDurationMin + 1;
        $convertedDurationSec = $convertedDurationSec % 60;
    }
    
    #force leading zero on seconds
    if ($convertedDurationSec < 9)
    {
        $convertedDurationSec = "0$convertedDurationSec";
    }

    $page->stringr($font_des, $takeFontSize, ($tableColumns[$columnIndex{'Duration'}]->{'RightLimit'}-$tableColumns[$columnIndex{'Duration'}]->{'TextOffset'}), $currentVertDrawPosn, "$convertedDurationMin\" $convertedDurationSec\'");

##### DRAW COMMENTS
    my $commentVertDrawPosn = $currentVertDrawPosn;
    my $commentSpaceUsed = 0;
    # for the number of elements in the array, print out each line
    foreach(@comments)
    {
      $page->string($font_des, $takeFontSize, ($tableColumns[$columnIndex{'Comments'}]->{'LeftLimit'}+$tableColumns[$columnIndex{'Comments'}]->{'TextOffset'}), $commentVertDrawPosn, $_);
      $commentVertDrawPosn = $commentVertDrawPosn - ($takeFontSize * $takeFontHeightFactor);
      $commentSpaceUsed = $commentSpaceUsed + ($takeFontSize * $takeFontHeightFactor);
    }

#### DRAW TC INFO
    my $tcVertDrawPosn = $currentVertDrawPosn;
    #Timecode information
    $page->stringc($font_des, $takeFontSize, $tableColumns[$columnIndex{'TC'}]->{'Centre'}, $tcVertDrawPosn, "in  : $inpoint");
    $tcVertDrawPosn = $tcVertDrawPosn - $tcVertLineSeparation;
    $page->stringc($font_des, $takeFontSize, $tableColumns[$columnIndex{'TC'}]->{'Centre'}, ($tcVertDrawPosn-($takeFontSize * $takeFontHeightFactor)),"out : $out");
    $tcVertDrawPosn = $tcVertDrawPosn - $tcVertLineSeparation;
    $page->stringc($font_des, $takeFontSize, $tableColumns[$columnIndex{'TC'}]->{'Centre'}, ($tcVertDrawPosn-($takeFontSize * $takeFontHeightFactor * 2)), "date:  $date");

#### WORK OUT SPACE USED AND DRAW BOUNDING LINE
    # ensure that the horizontal bounding line is below the date if comment and dia range take up less space than the timecode column elements
    # set space used to that of the TC info
    my $tcSpaceUsed = ($takeFontSize * $takeFontHeightFactor * 2) +($tcVertLineSeparation * 2);

    if($commentSpaceUsed > $tcSpaceUsed) # the timecode takes up more space
    {
        $tcSpaceUsed = $commentSpaceUsed;
    }

    if ($seqRangeSpaceUsed > $tcSpaceUsed)
    {
       $tcSpaceUsed = $seqRangeSpaceUsed;
    }
    # take maximum space used off the current draw posn (first line of take)
    $currentVertDrawPosn = $currentVertDrawPosn - $tcSpaceUsed;
    
    # now at lowest point of take info
    # move to position for horizontal bounding line
    $currentVertDrawPosn = $currentVertDrawPosn - $boundingLineVertSeparation;
}
