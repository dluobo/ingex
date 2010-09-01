#!/bin/sh

# this script can be called by the qc_player just prior to quiting a session


OUTPUT_LOG_FILENAME="/tmp/qcreportout.txt"


# create a KDE progress bar dialog
REF=$(kdialog --progressbar "Processing QC session" 100)


# generate report
if ! (qc_report --print-qc-url --progress-ref "$REF" --progress-start 0 --progress-end 95 $@ > $OUTPUT_LOG_FILENAME)
then
	if [[ $REF =~ ^org.kde.kdialog ]]
	then
		qdbus $REF close
	else
		dcop $REF close
	fi
	echo "Failed to process qc session"
	kdialog --title "Failed to process QC session" --passivepopup " " 4
	exit 1
fi


# done creating report
if [[ $REF =~ ^org.kde.kdialog ]]
then
	qdbus $REF Set org.kde.kdialog.ProgressDialog value 100
	qdbus $REF setLabelText "Complete"
	sleep 1
	qdbus $REF close
else
	dcop $REF setProgress 100
	dcop $REF setLabel "Complete"
	sleep 1
	dcop $REF close
fi


# open firefox with report for printing
REPORT_FILENAME=$(cat $OUTPUT_LOG_FILENAME)
if [ "$REPORT_FILENAME" != "" ]
then
	firefox "$REPORT_FILENAME"
fi

