/*
 * $Id: qc_http_access.c,v 1.8 2011/06/14 15:44:39 philipn Exp $
 *
 *
 *
 * Copyright (C) 2008-2011 British Broadcasting Corporation, All Rights Reserved
 *
 * Author: Philip de Nier
 *         Tom Heritage
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#include "qc_http_access.h"


#if !defined(HAVE_SHTTPD)

int qch_create_qc_http_access(MediaPlayer* player, int port, const char* cacheDirectory,
    const char* reportDirectory, QCHTTPAccess** access)
{
    return 0;
}

void qch_free_qc_http_access(QCHTTPAccess** access)
{
    return;
}


#else

#include <shttpd.h>

#include "qc_session.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


struct QCHTTPAccess
{
    char* cacheDirectory;
    char* reportDirectory;

    pthread_t httpThreadId;
    int stopped;

    struct shttpd_ctx* ctx;
};

static const char* g_qcReportFramed =
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
    "<html>\n"
    "<head>\n"
    "	<title>QC Report</title>\n"
    "	<meta HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\"/>\n"
    "</head>\n"
    "<frameset rows=\"15%, 100%\" frameborder=\"0\">\n"
    "	<frame name=\"controlframe\" src=\"control.html?report=$$1&session=$$2&carrier=$$3\" scrolling=\"0\"/>\n"
    "	<frame name=\"reportframe\" src=\"/$$1\"/>\n"
    "	<noframe> \n"
    "		<p>\n"
    "			<a href=\"report.html?report=$$1\">QC Report</a> \n"
    "		</p> \n"
    "	</noframe> \n"
    "</frameset>\n"
    "</html>\n"
    "\n"
    "";

static const char* qc_qcReportControl =
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
    "<html>\n"
    "<head>\n"
    "	<title>QC Report Control</title>\n"
    "	<meta HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\"/>\n"
    "	\n"
    "	<style type=\"text/css\">\n"
    "		body {\n"
    "			background-color: #F0F0F0;\n"
    "		}\n"
    "		td {\n"
    "			padding-right: 10px;\n"
    "			vertical-align: top;\n"
    "			text-align: center;\n"
    "		}\n"
    "		td#print {\n"
    "			padding-right: 30px;\n"
    "		}\n"
    "		span#comments-comment {\n"
    "			font-size: 70%;\n"
    "		}\n"
    "		input.control-button {\n"
    "			font-size: 110%;\n"
    "		}\n"
    "	</style>	\n"
    "\n"
    "    <script type=\"text/javascript\">\n"
    "	<!--\n"
    "	\n"
    "	var commentsUpdateRequest = null;\n"
    "	var enableCommentsUpdateRequest = true;\n"
    "	\n"
    "	\n"	
    "	var finalsessionrecordSearchRequest = null;\n"
    "	var finalsessionrecordWriteRequest = null;\n"
    "	\n"	
    "	var enableSubmitandPrintRequest = true;\n"
    "	\n"
    "	function get_page_parameter(name)\n"
    "	{\n"
    "		var nameAndValue;\n"
    "		var regex = /[\\?&]([^=]+)=([^=&]*)/g;\n"
    "		regex.lastIndex = 0; // reset the regex to avoid missing matches when calling this function again\n"
    "		while ((nameAndValue = regex.exec(window.location.href)) != null)\n"
    "		{\n"
    "			if (decodeURI(nameAndValue[1]) == name)\n"
    "			{\n"
    "				return decodeURI(nameAndValue[2]);\n"
    "			}\n"
    "		}\n"
    "		\n"
    "		return \"\";\n"
    "	}\n"
    "	\n"
    "	function fill_in_comments()\n"
    "	{\n"
    "		var reportCommentsE = parent.frames.reportframe.document.getElementById(\"comments\");\n"
    "		var controlCommentsE = document.getElementById(\"comments\");\n"
    "		\n"
    "		controlCommentsE.innerHTML = reportCommentsE.innerHTML.replace(/\\<br\\/\\>/g, \"\\n\");\n"
    "	}\n"
    "	\n"
    "	function update_comments_handler()\n"
    "	{\n"
    "		try\n"
    "		{\n"
    "			if (commentsUpdateRequest.readyState == 4)\n"
    "			{\n"
    "				// reload report frame whatever the result is of the request\n"
    "				parent.frames.reportframe.location.reload();\n"
    "				\n"
    "				enableCommentsUpdateRequest = true;\n"
    "			}\n"
    "		} \n"
    "		catch (err)\n"
    "		{\n"
    "			enableCommentsUpdateRequest = true;\n"
    "		}\n"
    "	}\n"
    "	\n"
    "	function update_comments()\n"
    "	{\n"
    "		if (enableCommentsUpdateRequest)\n"
    "		{\n"
    "			enableCommentsUpdateRequest = false;\n"
    "			\n"
    "			try\n"
    "			{\n"
    "				var controlCommentsE = document.getElementById(\"comments\");\n"
    "		\n"
    "				var comments = controlCommentsE.value;		\n"
    "				var reportName = get_page_parameter(\"report\");\n"
    "				var sessionName = get_page_parameter(\"session\");\n"
    "				var carrierName = get_page_parameter(\"carrier\");\n"
    "\n"
    "				commentsUpdateRequest = new XMLHttpRequest();\n"
    "				commentsUpdateRequest.open(\"POST\", \"/qcreport/comments/update\", true);\n"
    "				commentsUpdateRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');    \n"
    "				commentsUpdateRequest.onreadystatechange = update_comments_handler;\n"
    "				commentsUpdateRequest.send(\n"
    "					\"comments=\" + encodeURIComponent(comments) +\n"
    "					\"&report=\" + encodeURIComponent(reportName) +\n"
    "					\"&session=\" + encodeURIComponent(sessionName) + \n"
    "					\"&carrier=\" + encodeURIComponent(carrierName));\n"
    "				\n"
    "			}\n"
    "			catch (err)\n"
    "			{\n"
    "				enableCommentsUpdateRequest = true;\n"
    "			}\n"
    "		}\n"
    "	}\n"
    "\n"
    "	function finalsessionrecordSearch_handler()\n"
    "	{\n"
    "		try\n"
    "		{\n"
    "			if (finalsessionrecordSearchRequest.readyState == 4)\n"
    "			{\n"
////    "		                alert(\"ready state is 4\");        		\n"
////    "				var controlCommentsE = document.getElementById(\"comments\");\n"
////    "				controlCommentsE.value = finalsessionrecordSearchRequest.responseText;		\n"
    "                       var user_OK_to_proceed = false;\n"
    "                       if (finalsessionrecordSearchRequest.responseText == \"FOUND\")\n" 
    "                       {\n"
    "                           user_OK_to_proceed = confirm(\"WARNING!!\\n\\nA QC report has already been submitted to the database for this MXF file. If you proceed then the existing QC report entry will be overwritten.\\n\\nPress 'OK' to proceed and overwrite the existing QC report entry. The QC report will then be printed.\\n\\nIf you press 'Cancel' then no submission will be made to the database and no report will be printed. \");\n"
    "                       }\n"

    "                       if ( (finalsessionrecordSearchRequest.responseText == \"NOT_FOUND\") || (user_OK_to_proceed == true) )\n"
    "                       {\n"
    "				var reportName = get_page_parameter(\"report\");\n"
    "				var sessionName = get_page_parameter(\"session\");\n"
    "				var carrierName = get_page_parameter(\"carrier\");\n"
    "\n"
    "				finalsessionrecordWriteRequest = new XMLHttpRequest();\n"
    "				finalsessionrecordWriteRequest.open(\"POST\", \"/qcreport/finalsessionrecord/write\", true);\n"
    "				finalsessionrecordWriteRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');    \n"
    "				finalsessionrecordWriteRequest.onreadystatechange = finalsessionrecordWrite_handler;\n"
    "				finalsessionrecordWriteRequest.send(\n"
    "					\"&report=\" + encodeURIComponent(reportName) +\n"
    "					\"&session=\" + encodeURIComponent(sessionName) + \n"
    "					\"&carrier=\" + encodeURIComponent(carrierName));\n"
    "				\n"
    "                       }\n"
    "                       else\n"
    "                       {\n"
    "                           alert(\"No submission has been made to the database and no report has been printed.\");\n"
    "			        enableSubmitandPrintRequest = true;\n"
    "                       }\n"
    "			}\n"
    "		} \n"
    "		catch (err)\n"
    "		{\n"
/////Do we need to change this error handling at all?

    "                   alert(\"An error has occurred while trying to Submit and Print the QC report. No report has been printed but a submission may have been made to the database.\");\n"    
    "			enableSubmitandPrintRequest = true;\n"
    "		}\n"
    "	}\n"
    "	\n"
    "	function finalsessionrecordWrite_handler()\n"
    "	{\n"
    "		try\n"
    "		{\n"
    "			if (finalsessionrecordWriteRequest.readyState == 4)\n"
    "			{\n"
    "			    if (finalsessionrecordWriteRequest.responseText == \"SUCCESS\")\n"
    "                       {\n"
    "                           parent.frames.reportframe.print()\n"
    "			    }\n"
    "                       else\n"
    "                       {\n"
    "                           alert(\"An error has occurred while trying to Submit and Print the QC report. No report has been printed but a submission may have been made to the database.\");\n"    
    "			    }\n"
    "			}\n"
    "		} \n"
    "		catch (err)\n"
    "		{\n"
/////Do we need to change this error handling at all?
    "                   alert(\"An error has occurred while trying to Submit and Print the QC report. No report has been printed but a submission may have been made to the database.\");\n"    
    "		}\n"
    "		enableSubmitandPrintRequest = true;\n"
    "	}\n"
    "	\n"
    "	function SubmitandPrint()\n"
    "	{\n"
    "		if (enableSubmitandPrintRequest)\n"
    "		{\n"
    "			enableSubmitandPrintRequest = false;\n"
    "			\n"
    "			try\n"
    "			{\n"
    "				var reportName = get_page_parameter(\"report\");\n"
    "				var sessionName = get_page_parameter(\"session\");\n"
    "				var carrierName = get_page_parameter(\"carrier\");\n"
    "\n"
    "				finalsessionrecordSearchRequest = new XMLHttpRequest();\n"
    "				finalsessionrecordSearchRequest.open(\"POST\", \"/qcreport/finalsessionrecord/search\", true);\n"
    "				finalsessionrecordSearchRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');    \n"
    "				finalsessionrecordSearchRequest.onreadystatechange = finalsessionrecordSearch_handler;\n"
    "				finalsessionrecordSearchRequest.send(\n"
    "					\"&report=\" + encodeURIComponent(reportName) +\n"
    "					\"&session=\" + encodeURIComponent(sessionName) + \n"
    "					\"&carrier=\" + encodeURIComponent(carrierName));\n"
    "				\n"
    "			}\n"
    "			catch (err)\n"
    "			{\n"
    "				enableSubmitandPrintRequest = true;\n"
    "                           alert(\"An error has occurred while trying to Submit and Print the QC report. No report has been printed and no submission has been made to the database.\");\n"
    "			}\n"
    "		}\n"
    "	}\n"
    "	\n"
    "	-->\n"
    "	</script>\n"
    "\n"
    "</head>\n"
    "<body onload=\"fill_in_comments()\">\n"
    "\n"
    "<table>\n"
    "<tbody>\n"
    "	<tr>\n"
    "		<td id=\"print\">\n"
    "			<input type=\"button\" value=\"Submit to&#13;&#10;Database&#13;&#10;and Print\" class=\"control-button\" accesskey=\"P\" onclick=\"parent.frames.reportframe.focus(); SubmitandPrint();\" onfocus=\"this.setAttribute('style', 'background-color:#FFFFD5')\" onblur=\"this.setAttribute('style', '')\"/>\n"
    "		</td>\n"
    "		<td>\n"
    "			<input type=\"button\" value=\"Update Comments\" class=\"control-button\" accesskey=\"U\" onclick=\"update_comments()\" onfocus=\"this.setAttribute('style', 'background-color:#FFFFD5')\" onblur=\"this.setAttribute('style', '')\"/><br/>\n"
    "			<span id=\"comments-comment\">(max 400 characters)</span>\n"
    "		</td>\n"
    "		<td>\n"
    "			<textarea id=\"comments\" rows=\"2\" cols=\"75\" accesskey=\"C\" onfocus=\"this.setAttribute('style', 'background-color:#ffff66')\" onblur=\"this.setAttribute('style', '')\"></textarea>\n"
    "		</td>\n"
    "	</tr>\n"
    "</tbody>\n"
    "</table>\n"
    "\n"
    "</body>\n"
    "</html>\n"
    "\n"
    "";


static const char* g_qcReportCommentStartMarker = "<!-- COMMENT START MARKER -->";
static const char* g_qcReportCommentEndMarker = "<!-- COMMENT END MARKER -->";

static const char* g_qcReportCommentHTML =
    "\n"
    "<br/>\n"
    "<table id=\"t4\" class=\"results\">\n"
    "<tbody>\n"
    "	<tr>\n"
    "		<td class=\"prop-name\">Comments</td>\n"
    "		<td id=\"comments\">$$1</td>\n"
    "	</tr>\n"
    "</tbody>\n"
    "</table>\n"
    "\n";

static const char* q_sessionCommentsHeaderStart = "# Comments";

static const char* q_sessionCommentsHeader =
    "# Comments, , , , , , , ,\n"
    "# , , , , , , , ,\n";



static int update_qc_report(QCHTTPAccess* access, const char* reportName, const char* comments)
{
    char reportFilename[FILENAME_MAX];
    char copyCommand[FILENAME_MAX]; /* we'll never get near FILENAME_MAX so this amount is ok */
    FILE* originalReport = NULL;
    FILE* modifiedReport = NULL;
    int modifiedReportFd = -1;
    char tempFilename[] = "/tmp/modqcreportXXXXXX";
    int c;
    int startMarkerIndex = 0;
    int endMarkerIndex = 0;
    int state = 0;
    int i, j;
    int templateLen;
    int commentsLen;
    int startMarkerLen = strlen(g_qcReportCommentStartMarker);
    int endMarkerLen = strlen(g_qcReportCommentEndMarker);


    /* open files */

    strcpy(reportFilename, access->reportDirectory);
    strcat_separator(reportFilename);
    strcat(reportFilename, reportName);

    originalReport = fopen(reportFilename, "rb");
    if (originalReport == NULL)
    {
        ml_log_error("Failed to open QC report '%s': %s\n", reportFilename, strerror(errno));
        goto fail;
    }

    modifiedReportFd = mkstemp(tempFilename);
    if (modifiedReportFd == -1)
    {
        ml_log_error("Failed to open temporary file '%s': %s\n", tempFilename, strerror(errno));
        goto fail;
    }
    modifiedReport = fdopen(modifiedReportFd, "wb");
    if (modifiedReport == NULL)
    {
        ml_log_error("Failed to associate with temporary file file descriptor: %s\n", strerror(errno));
        goto fail;
    }
    modifiedReportFd = -1;


    /* update report */

    while ((c = fgetc(originalReport)) != EOF)
    {
        switch (state)
        {
            /* copy until the start of the comments sections and then insert
            the modified comments html */
            case 0:
                fputc(c, modifiedReport);

                if (c == g_qcReportCommentStartMarker[startMarkerIndex])
                {
                    if (startMarkerIndex + 1 == startMarkerLen)
                    {
                        fputc('\n', modifiedReport);

                        /* copy the new comments section with html escaped comments */
                        templateLen = strlen(g_qcReportCommentHTML);
                        for (i = 0; i < templateLen; i++)
                        {
                            if (i + 2 < templateLen &&
                                g_qcReportCommentHTML[i] == '$' &&
                                g_qcReportCommentHTML[i + 1] == '$')
                            {
                                if (g_qcReportCommentHTML[i + 2] == '1')
                                {
                                    commentsLen = strlen(comments);
                                    for (j = 0; j < commentsLen; j++)
                                    {
                                        if (comments[j] == '<')
                                        {
                                            fprintf(modifiedReport, "&lt;");
                                        }
                                        else if (comments[j] == '>')
                                        {
                                            fprintf(modifiedReport, "&gt;");
                                        }
                                        else if (comments[j] == '&')
                                        {
                                            fprintf(modifiedReport, "&amp;");
                                        }
                                        else if (comments[j] == '\n')
                                        {
                                            fprintf(modifiedReport, "<br/>\n");
                                        }
                                        else
                                        {
                                            fputc(comments[j], modifiedReport);
                                        }
                                    }
                                }

                                i += 2;
                            }
                            else
                            {
                                fputc(g_qcReportCommentHTML[i], modifiedReport);
                            }
                        }

                        state = 1;
                    }
                    else
                    {
                        startMarkerIndex++;
                    }
                }
                else
                {
                    startMarkerIndex = 0;
                }

                break;

            /* seek to the end of the original comments section */
            case 1:
                if (c == g_qcReportCommentEndMarker[endMarkerIndex])
                {
                    if (endMarkerIndex + 1 == endMarkerLen)
                    {
                        fprintf(modifiedReport, "%s\n", g_qcReportCommentEndMarker);
                        state = 2;
                    }
                    else
                    {
                        endMarkerIndex++;
                    }
                }
                else
                {
                    endMarkerIndex = 0;
                }

                break;

            /* copy the data after the comments section */
            case 2:
                fputc(c, modifiedReport);
                break;
        }
    }

    fclose(originalReport);
    originalReport = 0;
    fclose(modifiedReport);
    modifiedReport = 0;


    /* replace the original report with the modified report */

    strcpy(copyCommand, "cp ");
    strcat(copyCommand, tempFilename);
    strcat(copyCommand, " \"");
    strcat(copyCommand, reportFilename);
    strcat(copyCommand, "\"");

    if (system(copyCommand) != 0)
    {
        ml_log_error("Failed to replace report with modified report ('%s'): %s", copyCommand, strerror(errno));
        goto fail;
    }

    return 1;

fail:
    if (originalReport != NULL)
    {
        fclose(originalReport);
    }
    if (modifiedReportFd >= 0)
    {
        close(modifiedReportFd);
    }
    if (modifiedReport != NULL)
    {
        fclose(modifiedReport);
    }
    return 0;
}

static int update_session_files(QCHTTPAccess* access, const char* sessionName, const char* carrierName, const char* comments)
{
    char sessionFilename[FILENAME_MAX];
    char backupDirectory[FILENAME_MAX];
    char copyCommand[FILENAME_MAX]; /* we'll never get near FILENAME_MAX so this amount is ok */
    FILE* sessionFile = NULL;
    int c;
    int commentsHeaderIndex = 0;
    int commentsHeaderStartLen = strlen(q_sessionCommentsHeaderStart);
    int startOfLine = 1;
    long filePos;
    int fd = -1;


    /* update the session file in the cache */


    /* open the file */

    strcpy(sessionFilename, access->cacheDirectory);
    strcat_separator(sessionFilename);
    strcat(sessionFilename, carrierName);
    strcat_separator(sessionFilename);
    strcat(sessionFilename, sessionName);

    sessionFile = fopen(sessionFilename, "rb+");
    if (sessionFile == NULL)
    {
        ml_log_error("Failed to open session file '%s' for update: %s\n", sessionFilename, strerror(errno));
        goto fail;
    }


    /* find the comments section */

    while ((c = fgetc(sessionFile)) != EOF)
    {
        if ((startOfLine || commentsHeaderIndex > 0) &&
            c == q_sessionCommentsHeaderStart[commentsHeaderIndex])
        {
            if (commentsHeaderIndex + 1 == commentsHeaderStartLen)
            {
                /* found it - seek back to the start of the comments section */
                fseek(sessionFile, -commentsHeaderStartLen, SEEK_CUR);
                break;
            }
            else
            {
                commentsHeaderIndex++;
            }
        }
        else
        {
            commentsHeaderIndex = 0;
        }

        if (c == '\n')
        {
            startOfLine = 1;
        }
        else
        {
            startOfLine = 0;
        }
    }

    /* write the comments header and comments */

    /* write a newline if the comments section was not found and the last char was not a newline */
    if (commentsHeaderIndex + 1 != commentsHeaderStartLen && c != '\n')
    {
        fputc('\n', sessionFile);
    }
    fprintf(sessionFile, "%s%s", q_sessionCommentsHeader, comments);


    /* truncate the file */
    filePos = ftell(sessionFile);
    fd = fileno(sessionFile);
    ftruncate(fd, filePos);


    fclose(sessionFile);
    sessionFile = NULL;



    /* backup session file */

    strcpy(backupDirectory, access->reportDirectory);
    strcat_separator(backupDirectory);
    strcat(backupDirectory, carrierName);
    strcat(backupDirectory, "_backup");
    strcat_separator(backupDirectory);

    strcpy(copyCommand, "cp \"");
    strcat(copyCommand, sessionFilename);
    strcat(copyCommand, "\" \"");
    strcat(copyCommand, backupDirectory);
    strcat(copyCommand, "\"");

    if (system(copyCommand) != 0)
    {
        ml_log_error("Failed to copy session file ('%s') for backup: %s", copyCommand, strerror(errno));
        goto fail;
    }

    return 1;

fail:
    if (sessionFile != NULL)
    {
        fclose(sessionFile);
    }
    return 0;
}



static int get_query_value(struct shttpd_arg* arg, const char* name, char* value, int valueSize)
{
    const char* queryString;
    value[0] = '\0';

    queryString = shttpd_get_env(arg, "QUERY_STRING");
    if (queryString == NULL)
    {
        return 0;
    }

    int result = shttpd_get_var(name, queryString, strlen(queryString), value, valueSize);
    if (result < 0 || result == valueSize)
    {
        return 0;
    }
    value[valueSize - 1] = '\0';

    return 1;
}

static int get_post_value(struct shttpd_arg* arg, const char* name, char* value, int valueSize)
{
    int result = shttpd_get_var(name, arg->in.buf, arg->in.len, value, valueSize);
    if (result < 0 || result == valueSize)
    {
        return 0;
    }
    value[valueSize - 1] = '\0';

    return 1;
}

static void send_bad_request(struct shttpd_arg* arg, const char* description)
{
    shttpd_printf(arg, "HTTP/1.1 400 %s\r\n\r\n", description);
    arg->flags |= SHTTPD_END_OF_OUTPUT;
}

static void send_server_error(struct shttpd_arg* arg, const char* description)
{
    shttpd_printf(arg, "HTTP/1.1 500 %s\r\n\r\n", description);
    arg->flags |= SHTTPD_END_OF_OUTPUT;
}


static void http_qcreport_framed_page(struct shttpd_arg* arg)
{
    char reportName[256];
    char sessionName[256];
    char carrierName[256];
    char page[2048];
    int i;
    int templateLen;
    int pageLen;
    int pageIndex;


    if (!get_query_value(arg, "report", reportName, sizeof(reportName)))
    {
        send_bad_request(arg, "Missing 'report' parameter");
        return;
    }

    if (!get_query_value(arg, "session", sessionName, sizeof(sessionName)))
    {
        send_bad_request(arg, "Missing 'session' parameter");
        return;
    }

    if (!get_query_value(arg, "carrier", carrierName, sizeof(carrierName)))
    {
        send_bad_request(arg, "Missing 'carrier' parameter");
        return;
    }


    /* fill in frames page template */

    templateLen = strlen(g_qcReportFramed);
    pageLen = sizeof(page) - 1;
    pageIndex = 0;
    for (i = 0; i < templateLen && pageIndex < pageLen; i++)
    {
        if (i + 2 < templateLen &&
            g_qcReportFramed[i] == '$' &&
            g_qcReportFramed[i + 1] == '$')
        {
            if (g_qcReportFramed[i + 2] == '1')
            {
                strcpy(&page[pageIndex], reportName);
                pageIndex += strlen(reportName);
            }
            else if (g_qcReportFramed[i + 2] == '2')
            {
                strcpy(&page[pageIndex], sessionName);
                pageIndex += strlen(sessionName);
            }
            else if (g_qcReportFramed[i + 2] == '3')
            {
                strcpy(&page[pageIndex], carrierName);
                pageIndex += strlen(carrierName);
            }

            i += 2;
        }
        else
        {
            page[pageIndex] = g_qcReportFramed[i];
            pageIndex++;
        }
    }
    page[pageLen] = '\0';


    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
    shttpd_printf(arg, "Content-Type: %s\r\n\r\n", "text/html");

    shttpd_printf(arg, "%s", page);

	arg->flags |= SHTTPD_END_OF_OUTPUT;
}

static void http_qcreport_control_page(struct shttpd_arg* arg)
{
    char reportName[256];
    char sessionName[256];
    char carrierName[256];

    if (!get_query_value(arg, "report", reportName, sizeof(reportName)))
    {
        send_bad_request(arg, "Missing 'report' parameter");
        return;
    }

    if (!get_query_value(arg, "session", sessionName, sizeof(sessionName)))
    {
        send_bad_request(arg, "Missing 'session' parameter");
        return;
    }

    if (!get_query_value(arg, "carrier", carrierName, sizeof(carrierName)))
    {
        send_bad_request(arg, "Missing 'carrier' parameter");
        return;
    }

    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
    shttpd_printf(arg, "Content-Type: %s\r\n\r\n", "text/html");

    shttpd_printf(arg, "%s", qc_qcReportControl);

	arg->flags |= SHTTPD_END_OF_OUTPUT;
}

static void http_qcreport_report_page(struct shttpd_arg* arg)
{
    QCHTTPAccess* access = (QCHTTPAccess*)arg->user_data;
    char report[256];
    char filename[FILENAME_MAX];
    FILE* reportFile;
	int	state = (int)((char *)arg->state - (char *)0);
    int numRead;
    int outNumBytes;

    if (!get_query_value(arg, "report", report, sizeof(report)))
    {
        send_bad_request(arg, "Missing 'report' parameter");
        return;
    }

    strcpy(filename, access->reportDirectory);
    strcat_separator(filename);
    strcat(filename, report);


    reportFile = fopen(filename, "rb");
    if (reportFile == NULL)
    {
        send_bad_request(arg, "Unknown report file");
        return;
    }
    if (fseek(reportFile, SEEK_SET, state) != 0)
    {
        if (state == 0)
        {
            send_server_error(arg, "Failed to seek in report file");
        }
        else
        {
            arg->flags |= SHTTPD_END_OF_OUTPUT;
        }
        fclose(reportFile);
        return;
    }

    outNumBytes = arg->out.num_bytes; /* store to allow recovery when an error occurs */
	if (state == 0)
    {
        shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
        shttpd_printf(arg, "Content-Type: %s\r\n\r\n", "text/html");
	}

    numRead = fread(&arg->out.buf[arg->out.num_bytes], 1, arg->out.len, reportFile);
    if (numRead == 0 && ferror(reportFile) != 0)
    {
        arg->out.num_bytes = outNumBytes; /* restore original */

        if (state == 0)
        {
            send_server_error(arg, "Failed to read report file");
        }
        else
        {
            arg->flags |= SHTTPD_END_OF_OUTPUT;
        }
        fclose(reportFile);
        return;
    }
    arg->out.num_bytes += numRead;
    state += numRead;


    fclose(reportFile);


	arg->state = (char *)0 + state;
    if (numRead < arg->out.len)
    {
        arg->flags |= SHTTPD_END_OF_OUTPUT;
    }
}

static void http_qcreport_comments_update_page(struct shttpd_arg* arg)
{
    QCHTTPAccess* access = (QCHTTPAccess*)arg->user_data;
    char reportName[256];
    char sessionName[256];
    char carrierName[256];
    char comments[MAX_SESSION_COMMENTS_SIZE];

    const char* requestMethod = shttpd_get_env(arg, "REQUEST_METHOD");
    if (strcmp(requestMethod, "POST") != 0)
    {
        send_bad_request(arg, "Not a POST request");
        return;
    }

    if (arg->flags & SHTTPD_MORE_POST_DATA)
    {
        /* wait for the rest of the post data */
        return;
    }


    if (!get_post_value(arg, "comments", comments, MAX_SESSION_COMMENTS_SIZE))
    {
        send_bad_request(arg, "Missing 'comments' parameter");
        return;
    }

    if (!get_post_value(arg, "report", reportName, sizeof(reportName)))
    {
        send_bad_request(arg, "Missing 'report' parameter");
        return;
    }

    if (!get_post_value(arg, "session", sessionName, sizeof(sessionName)))
    {
        send_bad_request(arg, "Missing 'session' parameter");
        return;
    }

    if (!get_post_value(arg, "carrier", carrierName, sizeof(carrierName)))
    {
        send_bad_request(arg, "Missing 'carrier' parameter");
        return;
    }
    if (!update_qc_report(access, reportName, comments) ||
        !update_session_files(access, sessionName, carrierName, comments))
    {
        send_server_error(arg, "Failed to update files");
        return;
    }

    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n\r\n");

	arg->flags |= SHTTPD_END_OF_OUTPUT;
}


static int finalsessionrecord_search(QCHTTPAccess* access, const char* reportName, const char* sessionName, const char* carrierName)
//////Return values:
//////  0   --   FAIL / ERROR
//////  1   --   A Final Session Record File exists for the specified MXF file
//////  2   --   No Final Session Record File exists for the specified MXF file
{
    int returnValue;
    char finalsessionrecordFilename_cachedir[FILENAME_MAX];
    finalsessionrecordFilename_cachedir[0]=0;
    char finalsessionrecordFilename_backupdir[FILENAME_MAX];
    finalsessionrecordFilename_backupdir[0]=0;
    char mxfFilePrefix[FILENAME_MAX];
    mxfFilePrefix[0]=0;
    char finalsessionrecordFilename[FILENAME_MAX];
    finalsessionrecordFilename[0]=0;

    char search_string[] = ".";
    int mxfFilePrefixLength = strcspn (reportName, search_string);
    strncpy (mxfFilePrefix, reportName, mxfFilePrefixLength);
    mxfFilePrefix[mxfFilePrefixLength]=0;              ////Omitting this line can lead to unpredictable behaviour.

    strcpy(finalsessionrecordFilename, mxfFilePrefix);
    strcat(finalsessionrecordFilename, "_final_session_record.txt");

    strcpy(finalsessionrecordFilename_cachedir, access->cacheDirectory);
    strcat_separator(finalsessionrecordFilename_cachedir);
    strcat(finalsessionrecordFilename_cachedir, carrierName);
    strcat_separator(finalsessionrecordFilename_cachedir);
    strcat(finalsessionrecordFilename_cachedir, finalsessionrecordFilename);

    strcpy(finalsessionrecordFilename_backupdir, access->reportDirectory);
    strcat_separator(finalsessionrecordFilename_backupdir);
    strcat(finalsessionrecordFilename_backupdir, carrierName);
    strcat(finalsessionrecordFilename_backupdir, "_backup");
    strcat_separator(finalsessionrecordFilename_backupdir);
    strcat(finalsessionrecordFilename_backupdir, finalsessionrecordFilename);

    //// We need to check both possible locations for the Final Session Record file.
    FILE* finalsessionrecordFile_cachedir = NULL;
    FILE* finalsessionrecordFile_backupdir = NULL;

    finalsessionrecordFile_cachedir = fopen(finalsessionrecordFilename_cachedir, "r");
    finalsessionrecordFile_backupdir = fopen(finalsessionrecordFilename_backupdir, "r");
    
    if ( (finalsessionrecordFile_cachedir == NULL) && (finalsessionrecordFile_backupdir == NULL) )
    {
        returnValue = 2;
    } 
    else
    {
        returnValue = 1;
    }
    
    if (finalsessionrecordFile_cachedir != NULL)
    {
        fclose (finalsessionrecordFile_cachedir);
    }

    if (finalsessionrecordFile_backupdir != NULL)
    {
        fclose (finalsessionrecordFile_backupdir);
    }
    
    return returnValue;
}


static void http_qcreport_finalsessionrecord_search_page(struct shttpd_arg* arg)
{
    QCHTTPAccess* access = (QCHTTPAccess*)arg->user_data;
    char reportName[256];
    char sessionName[256];
    char carrierName[256];

    const char* requestMethod = shttpd_get_env(arg, "REQUEST_METHOD");
    if (strcmp(requestMethod, "POST") != 0)
    {
        send_bad_request(arg, "Not a POST request");
        return;
    }

    if (arg->flags & SHTTPD_MORE_POST_DATA)
    {
        /* wait for the rest of the post data */
        return;
    }


    if (!get_post_value(arg, "report", reportName, sizeof(reportName)))
    {
        send_bad_request(arg, "Missing 'report' parameter");
        return;
    }

    if (!get_post_value(arg, "session", sessionName, sizeof(sessionName)))
    {
        send_bad_request(arg, "Missing 'session' parameter");
        return;
    }


    if (!get_post_value(arg, "carrier", carrierName, sizeof(carrierName)))
    {
        send_bad_request(arg, "Missing 'carrier' parameter");
        return;
    }
    
    int finalsessionrecord_search_result = finalsessionrecord_search(access, reportName, sessionName, carrierName);
    

    if ( finalsessionrecord_search_result == 1 )
    {
        shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
        shttpd_printf(arg, "Content-Type: %s\r\n\r\n", "text/plain");
        shttpd_printf(arg, "FOUND");
        arg->flags |= SHTTPD_END_OF_OUTPUT;
    }
    else if ( finalsessionrecord_search_result == 2 )
    {
        shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
        shttpd_printf(arg, "Content-Type: %s\r\n\r\n", "text/plain");
        shttpd_printf(arg, "NOT_FOUND");
        arg->flags |= SHTTPD_END_OF_OUTPUT;
    }
    else
    {
        send_server_error(arg, "Failed to search for Final Session Record files");
    }
}


static int finalsessionrecord_write(QCHTTPAccess* access, const char* reportName, const char* sessionName, const char* carrierName)
//////Return values:
//////  0   --   FAIL / ERROR
//////  1   --   Final Session Record files created successfuly.
{
    //Run a check to be sure that sessionName and carrierName are valid. Note that this does NOT ensure that
    //they relate to the report identified by reportName. However, the level of checking required to ensure that
    //would go beyond the level of checking of these parameters that has been implemented elsewhere in this C file.
    char sessionFilename[FILENAME_MAX];
    sessionFilename[0]=0;
    FILE* sessionFile = NULL;
    strcpy(sessionFilename, access->cacheDirectory);
    strcat_separator(sessionFilename);
    strcat(sessionFilename, carrierName);
    strcat_separator(sessionFilename);
    strcat(sessionFilename, sessionName);
    sessionFile = fopen(sessionFilename, "r");
    if (sessionFile == NULL)
    {
        return 0;
    }
    else
    {
        fclose(sessionFile);
    }

    //Check that the report identified by reportName does exist.
    char reportFilename[FILENAME_MAX];
    reportFilename[0]=0;
    FILE* reportFile = NULL;
    strcpy(reportFilename, access->reportDirectory);
    strcat_separator(reportFilename);
    strcat(reportFilename, reportName);
    reportFile = fopen(reportFilename, "r");
    if (reportFile == NULL)
    {
        return 0;
    }
    else
    {
        fclose(reportFile);
    }


    //Generate the filenames etc
    char finalsessionrecordFilename_cachedir[FILENAME_MAX];
    finalsessionrecordFilename_cachedir[0]=0;
    char finalsessionrecordFilename_backupdir[FILENAME_MAX];
    finalsessionrecordFilename_backupdir[0]=0;
    char mxfFilePrefix[FILENAME_MAX];
    mxfFilePrefix[0]=0;
    char finalsessionrecordFilename[FILENAME_MAX];
    finalsessionrecordFilename[0]=0;
    
    char search_string[] = ".";
    int mxfFilePrefixLength = strcspn (reportName, search_string);
    strncpy (mxfFilePrefix, reportName, mxfFilePrefixLength);
    mxfFilePrefix[mxfFilePrefixLength]=0;        ////Omitting this line can lead to unpredictable behaviour.
    strcpy(finalsessionrecordFilename, mxfFilePrefix);
    strcat(finalsessionrecordFilename, "_final_session_record.txt");
   
    
    strcpy(finalsessionrecordFilename_cachedir, access->cacheDirectory);
    strcat_separator(finalsessionrecordFilename_cachedir);
    strcat(finalsessionrecordFilename_cachedir, carrierName);
    strcat_separator(finalsessionrecordFilename_cachedir);
    strcat(finalsessionrecordFilename_cachedir, finalsessionrecordFilename);

    strcpy(finalsessionrecordFilename_backupdir, access->reportDirectory);
    strcat_separator(finalsessionrecordFilename_backupdir);
    strcat(finalsessionrecordFilename_backupdir, carrierName);
    strcat(finalsessionrecordFilename_backupdir, "_backup");
    strcat_separator(finalsessionrecordFilename_backupdir);
    strcat(finalsessionrecordFilename_backupdir, finalsessionrecordFilename);


    //Record the status and contents of any existing Final Session Record files before we change them
    FILE* finalsessionrecordFile_cachedir = NULL;
    FILE* finalsessionrecordFile_backupdir = NULL;

    char finalsessionrecordFileOrigContents_cachedir[FILENAME_MAX];
    finalsessionrecordFileOrigContents_cachedir[0]=0;
    char finalsessionrecordFileOrigContents_backupdir[FILENAME_MAX];
    finalsessionrecordFileOrigContents_backupdir[0]=0;

    int finalsessionrecordFileOrigExisted_cachedir = 0; 
    int finalsessionrecordFileOrigExisted_backupdir = 0; 

    finalsessionrecordFile_cachedir = fopen(finalsessionrecordFilename_cachedir, "r");
    finalsessionrecordFile_backupdir = fopen(finalsessionrecordFilename_backupdir, "r");


    if (finalsessionrecordFile_cachedir != NULL)
    {
        finalsessionrecordFileOrigExisted_cachedir = 1; 
        fgets ( finalsessionrecordFileOrigContents_cachedir, FILENAME_MAX, finalsessionrecordFile_cachedir); //Only worry about the first line of the file
        fclose (finalsessionrecordFile_cachedir);
    }

    if (finalsessionrecordFile_backupdir != NULL)
    {
        finalsessionrecordFileOrigExisted_backupdir = 1; 
        fgets ( finalsessionrecordFileOrigContents_backupdir, FILENAME_MAX, finalsessionrecordFile_backupdir); //Only worry about the first line of the file
        fclose (finalsessionrecordFile_backupdir);
    }


    //Change / create the Final Session Record Files
    finalsessionrecordFile_cachedir = fopen(finalsessionrecordFilename_cachedir, "w");
    finalsessionrecordFile_backupdir = fopen(finalsessionrecordFilename_backupdir, "w");
    
    if ( (finalsessionrecordFile_cachedir == NULL) || (finalsessionrecordFile_backupdir == NULL) )
    {
        goto fail;
    } 

    char session_timestamp[FILENAME_MAX];
    session_timestamp[0]=0;
    char * timestamp_locator;
    timestamp_locator = strstr (const_cast<char *>(sessionName), ".mxf_qcsession_");

    if ( timestamp_locator == NULL)
    {
        goto fail;
    } 

    timestamp_locator = timestamp_locator + 15;
    
    strncpy (session_timestamp, timestamp_locator, 15);

    if ( fputs (session_timestamp, finalsessionrecordFile_cachedir) < 0 )
    {
        goto fail;
    }

    if ( fputs (session_timestamp, finalsessionrecordFile_backupdir) < 0 )
    {
        goto fail;
    }
 
    fclose (finalsessionrecordFile_cachedir);
    fclose (finalsessionrecordFile_backupdir);

    return 1;
 

fail:
        
    if (finalsessionrecordFile_cachedir != NULL)
    {
        fclose (finalsessionrecordFile_cachedir);
    }

    if (finalsessionrecordFile_backupdir != NULL)
    {
        fclose (finalsessionrecordFile_backupdir);
    }

    //Now make sure that both Final Session Record files are returned to how they were

    finalsessionrecordFile_cachedir = fopen(finalsessionrecordFilename_cachedir, "r");
    finalsessionrecordFile_backupdir = fopen(finalsessionrecordFilename_backupdir, "r");

    char remove_command[FILENAME_MAX];
    remove_command[0]=0;

    if ( (finalsessionrecordFile_cachedir == NULL) && (finalsessionrecordFileOrigExisted_cachedir == 1) )
    {
        fclose (finalsessionrecordFile_cachedir);
        finalsessionrecordFile_cachedir = fopen(finalsessionrecordFilename_cachedir, "w");
        if ( finalsessionrecordFile_cachedir != NULL)
        {
            fputs (finalsessionrecordFileOrigContents_cachedir, finalsessionrecordFile_cachedir);
            fclose (finalsessionrecordFile_cachedir);
        }
    }
    else if ( (finalsessionrecordFile_cachedir != NULL) && (finalsessionrecordFileOrigExisted_cachedir == 0) )
    {
        fclose (finalsessionrecordFile_cachedir);
        strcpy(remove_command, "rm \"");
        strcat(remove_command, finalsessionrecordFilename_cachedir);
        strcat(remove_command, "\"");
        system(remove_command);
    }
    else if ( (finalsessionrecordFile_cachedir != NULL) && (finalsessionrecordFileOrigExisted_cachedir == 1) )
    {
        fclose (finalsessionrecordFile_cachedir);
        finalsessionrecordFile_cachedir = fopen(finalsessionrecordFilename_cachedir, "w");
        if ( finalsessionrecordFile_cachedir != NULL)
        { 
            fputs (finalsessionrecordFileOrigContents_cachedir, finalsessionrecordFile_cachedir);
            fclose (finalsessionrecordFile_cachedir);
        }
    }


    if ( (finalsessionrecordFile_backupdir == NULL) && (finalsessionrecordFileOrigExisted_backupdir == 1) )
    {
        fclose (finalsessionrecordFile_backupdir);
        finalsessionrecordFile_backupdir = fopen(finalsessionrecordFilename_backupdir, "w");
        if ( finalsessionrecordFile_backupdir != NULL)
        { 
            fputs (finalsessionrecordFileOrigContents_backupdir, finalsessionrecordFile_backupdir);
            fclose (finalsessionrecordFile_backupdir);
        }
    }
    else if ( (finalsessionrecordFile_backupdir != NULL) && (finalsessionrecordFileOrigExisted_backupdir == 0) )
    {
        fclose (finalsessionrecordFile_backupdir);
        strcpy(remove_command, "rm \"");
        strcat(remove_command, finalsessionrecordFilename_backupdir);
        strcat(remove_command, "\"");
        system(remove_command);
    }
    else if ( (finalsessionrecordFile_backupdir != NULL) && (finalsessionrecordFileOrigExisted_backupdir == 1) )
    {
        fclose (finalsessionrecordFile_backupdir);
        finalsessionrecordFile_backupdir = fopen(finalsessionrecordFilename_backupdir, "w");
        if ( finalsessionrecordFile_backupdir != NULL)
        { 
            fputs (finalsessionrecordFileOrigContents_backupdir, finalsessionrecordFile_backupdir);
            fclose (finalsessionrecordFile_backupdir);
        }
    }

    return 0;

}


static void http_qcreport_finalsessionrecord_write_page(struct shttpd_arg* arg)
{
    QCHTTPAccess* access = (QCHTTPAccess*)arg->user_data;
    char reportName[256];
    char sessionName[256];
    char carrierName[256];

    const char* requestMethod = shttpd_get_env(arg, "REQUEST_METHOD");
    if (strcmp(requestMethod, "POST") != 0)
    {
        send_bad_request(arg, "Not a POST request");
        return;
    }

    if (arg->flags & SHTTPD_MORE_POST_DATA)
    {
        /* wait for the rest of the post data */
        return;
    }


    if (!get_post_value(arg, "report", reportName, sizeof(reportName)))
    {
        send_bad_request(arg, "Missing 'report' parameter");
        return;
    }

    if (!get_post_value(arg, "session", sessionName, sizeof(sessionName)))
    {
        send_bad_request(arg, "Missing 'session' parameter");
        return;
    }


    if (!get_post_value(arg, "carrier", carrierName, sizeof(carrierName)))
    {
        send_bad_request(arg, "Missing 'carrier' parameter");
        return;
    }
    
    int finalsessionrecord_write_result = finalsessionrecord_write(access, reportName, sessionName, carrierName);
    

    if ( finalsessionrecord_write_result == 1 )
    {
        shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
        shttpd_printf(arg, "Content-Type: %s\r\n\r\n", "text/plain");
        shttpd_printf(arg, "SUCCESS");
        arg->flags |= SHTTPD_END_OF_OUTPUT;
    }
    else
    {
        send_server_error(arg, "Failed to write Final Session Record files");
    }
}


static void* http_thread(void* arg)
{
    QCHTTPAccess* access = (QCHTTPAccess*)arg;

    while (!access->stopped)
    {
        shttpd_poll(access->ctx, 1000);
    }

    shttpd_fini(access->ctx);

    pthread_exit((void*) 0);
}



int qch_create_qc_http_access(MediaPlayer* player, int port, const char* cacheDirectory,
    const char* reportDirectory, QCHTTPAccess** access)
{
    QCHTTPAccess* newAccess;

    CALLOC_ORET(newAccess, QCHTTPAccess, 1);

    CALLOC_OFAIL(newAccess->cacheDirectory, char, strlen(cacheDirectory) + 1);
    strcpy(newAccess->cacheDirectory, cacheDirectory);
    CALLOC_OFAIL(newAccess->reportDirectory, char, strlen(reportDirectory) + 1);
    strcpy(newAccess->reportDirectory, reportDirectory);

    CHK_OFAIL((newAccess->ctx = shttpd_init(NULL, "document_root", reportDirectory, NULL)) != NULL);
    shttpd_register_uri(newAccess->ctx, "/qcreport/framed.html", &http_qcreport_framed_page, newAccess);
    shttpd_register_uri(newAccess->ctx, "/qcreport/control.html", &http_qcreport_control_page, newAccess);
    shttpd_register_uri(newAccess->ctx, "/qcreport/report.html", &http_qcreport_report_page, newAccess);
    shttpd_register_uri(newAccess->ctx, "/qcreport/comments/update", &http_qcreport_comments_update_page, newAccess);
    shttpd_register_uri(newAccess->ctx, "/qcreport/finalsessionrecord/search", &http_qcreport_finalsessionrecord_search_page, newAccess);
    shttpd_register_uri(newAccess->ctx, "/qcreport/finalsessionrecord/write", &http_qcreport_finalsessionrecord_write_page, newAccess);
    CHK_OFAIL(shttpd_listen(newAccess->ctx, port, 0));

    CHK_OFAIL(create_joinable_thread(&newAccess->httpThreadId, http_thread, newAccess));


    *access = newAccess;
    return 1;

fail:
    qch_free_qc_http_access(&newAccess);
    return 0;
}

void qch_free_qc_http_access(QCHTTPAccess** access)
{
    if (*access == NULL)
    {
        return;
    }

    (*access)->stopped = 1;
    join_thread(&(*access)->httpThreadId, NULL, NULL);

    SAFE_FREE(&(*access)->cacheDirectory);
    SAFE_FREE(&(*access)->reportDirectory);

    SAFE_FREE(access);
}



#endif


