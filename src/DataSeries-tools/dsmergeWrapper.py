#!/usr/bin/python

import sys, os, subprocess, shlex
from optparse import OptionParser
from Crypto.Cipher import AES
import struct
import hashlib
import logging, time

def getTraceInfo(inputFile):
	if inputFile.endswith(".ds") == False:
		return
	tmpList = inputFile.split('_')
	runNum = tmpList[1]
	pipelineNum = int(tmpList[2][1:])
	maxPipeline = traceDict.get(runNum)
	if maxPipeline == None:
		traceDict[runNum] = pipelineNum
	elif pipelineNum > maxPipeline:
		traceDict[runNum] = pipelineNum

def parseCommandLine():
	parser = OptionParser()
	parser.add_option("-i", "--inputDir", dest="inputDir", type="string",
		help="directory where unmerged DataSeries traces are stored (REQUIRED)", 
		metavar="DIR")
	parser.add_option("-o", "--outputDir", dest="outputDir", type="string",
		help="directory where merged traces get stored (REQUIRED)", 
		metavar="DIR")
	parser.add_option("-c", "--chunkSize", dest="chunkSize", type="int",
		help="merged trace file size in KB (default 4000)")

	(options, args) = parser.parse_args()
	if options.inputDir == None or options.outputDir == None:
		parser.print_help()
		exit(1)
	if options.chunkSize == None:
		options.chunkSize = 4000000
	return (options.inputDir, options.outputDir, options.chunkSize)

# process command line arguments
(inputDir, outputDir, chunkSize) = parseCommandLine()
if os.path.exists(inputDir) and os.path.exists(outputDir):
	print 'inputDir:{}   outputDir:{}'.format(inputDir, outputDir)
	print '================================================='
else:
	print 'ERROR: input or output directory does not exist!'
	exit(1)
logging.basicConfig(filename=os.path.join(outputDir, 'dsmerge.log'),level=logging.DEBUG)
traceDict = {};

# get info for all traces in the inputDir
for f in os.listdir(inputDir):
	getTraceInfo(f)
# verify the existence of traces for all pipelines
for key in traceDict.keys():
	for i in range(traceDict[key]):
		traceFile = 'chronicle_' + key + '_p' + str(i) + '_000000.ds'
		if not os.path.exists(os.path.join(inputDir, traceFile)):
			logging.debug('ERROR: {} does not exist'.format(os.path.join(inputDir, traceFile)))
			traceDict[key] = -1
# merge all good traces
for key in traceDict.keys():
	traceFile = 'chronicle_' + key + '_00001.ds' # workaround because dsmerge gets stuck!
	if traceDict[key] > 0 and not os.path.exists(os.path.join(outputDir, traceFile)):
		#command = './dsmerge -i ' + inputDir + ' -p chronicle_' + key + ' -n ' + str(traceDict[key] + 1) + ' -o ' + outputDir + ' -s' + str(chunkSize)
		command = './dsmerge -i ' + inputDir + ' -p chronicle_' + key + ' -n ' + str(traceDict[key] + 1) + ' -o ' + outputDir	
		print command
		try:
			subprocess.check_call(shlex.split(command))
		except subprocess.CalledProcessError as e:
			logging.debug('ERROR: [{}] {} {} {}'.format(time.strftime(
				"%H:%M:%S %m:%d:%Y", time.localtime()),
				e.cmd, e.returncode, e.output))
