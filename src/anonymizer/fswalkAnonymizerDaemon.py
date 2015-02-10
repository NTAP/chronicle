#!/usr/bin/python
"""
A wrapper for anonymizing and encrypting file system walk traces
"""

import sys, os, subprocess, shlex
from optparse import OptionParser
import pyinotify
from pyinotify import WatchManager, ThreadedNotifier, EventsCodes, ProcessEvent
import struct
import hashlib
import re
import threading
import logging, time

class FileCloseCb(pyinotify.ProcessEvent):
	def process_IN_CLOSE_WRITE(self, event):
		if event.name.endswith(".ds") == False or os.path.exists(os.path.join(outputDir, event.name)):
			return
		anonymizeFile(event.path, event.name)

def runCommand(command):
	try:
		return subprocess.check_output(shlex.split(command))
	except subprocess.CalledProcessError as e:
		logging.debug('ERROR: [{}] {} {} {}'.format(time.strftime("%H:%M:%S %m:%d:%Y", time.localtime()),
			e.cmd, e.returncode, e.output))
		return 'ERROR'

class FS_Walk_Anonymizer(threading.Thread):
	def __init__(self, inputDir, inputFile):
		threading.Thread.__init__(self)
		self.inputDir = inputDir
		self.inputFile = inputFile

	def run(self):
		anonymizeFile(self.inputDir, self.inputFile)

def anonymizeFile(inputDir, inputFile):
	if inputFile.endswith(".ds") == False:
		return
	print '-------------------\nanonymizing %s' % os.path.join(inputDir, inputFile)
	command = './anonymizeSnapshot -c lzf --infile=' + os.path.join(inputDir, inputFile) + ' --outfile=' + os.path.join(outputDir, inputFile + '.anon') + ' -k ' + key 
	print command
	runCommand(command)
	if passphrase != None:
		print '-------------------\nencrypting %s' % os.path.join(outputDir, inputFile + '.anon')
		encryptAES_GPG(os.path.join(outputDir, inputFile + '.anon'))
		if validateAES_GPG(outputDir, inputFile + '.anon.gpg'):
			print 'Encryption of %s is validated' % os.path.join(outputDir, inputFile + '.anon.gpg')
			command = 'rm ' + os.path.join(outputDir, inputFile + '.anon')				
			runCommand(command)
		else:
			print 'FAILED encryption for %s' % os.path.join(outputDir, inputFile + '.anon.enc')
			logging.debug('ERROR: failed encryption for {}'.format(os.path.join(outputDir, inputFile + '.anon.enc')))
	if remove:
		command = 'rm ' + os.path.join(inputDir, inputFile)
		print command
		runCommand(command)

def encryptAES_GPG(inputFile):
	outputFile = inputFile + '.gpg'
	key256 = re.sub(r'\'|\"', 'X', hashlib.sha256(passphrase).digest())
	command = 'gpg --symmetric --cipher-algo aes -z0 --passphrase ' + key256 + ' -o ' + outputFile + ' ' + inputFile
	print command
	runCommand(command)

def validateAES_GPG(directory, fileName):
	return True #to save CPU resources
	print '===================\nvalidating encryption of %s' % os.path.join(fileName)
	command = './decryptTraces -g -f ' +  os.path.join(directory, fileName) + ' -p ' + passphrase + ' -o ' + directory
	print command
	if runCommand(command) != 'Error':
		outputFile = os.path.join(directory, fileName.split('.',1)[0] + '.ds')
		command = 'diff ' + outputFile + ' ' + re.sub('\.gpg$', '', os.path.join(directory, fileName))
		print command
		if len(runCommand(command).split()) > 0:
			return False
		command = 'rm ' + outputFile
		runCommand(command)
	else:
		return False
	return True

def parseCommandLine():
	parser = OptionParser()
	parser.add_option("-i", "--inputDir", dest="inputDir", type="string",
		help="directory where DataSeries traces are stored (REQUIRED)", 
		metavar="DIR")
	parser.add_option("-o", "--outputDir", dest="outputDir", type="string",
		help="directory where anonymized traces are stored (REQUIRED)", 
		metavar="DIR")
	parser.add_option("-k", "--key", dest="key", type="string",
		help="anonymization and encryption key (REQUIRED)", 
		metavar="FILE")
	parser.add_option("-p", "--passphrase", dest="passphrase", type="string", 
		help="symmetric encryption using the passphrase",
		metavar="STRING")
	parser.add_option("-n", action="store_false", dest="watch", default=True,
		help="run in the non-watch mode")
	parser.add_option("-r", action="store_true", dest="remove", default=False,
		help="remove input files after anonymization")
	parser.add_option("-t", "--threads", dest="threads", type="int")

	(options, args) = parser.parse_args()
	if options.inputDir == None or options.outputDir == None or options.key == None:
		parser.print_help()
		exit(1)
	if options.inputDir == options.outputDir and options.watch == True:
		print 'ERROR: input and output directories have to be different!'
		exit(1)
	return (options.inputDir, options.outputDir, options.passphrase, options.key,
		options.watch, options.remove, options.threads)

# process command line arguments
(inputDir, outputDir, passphrase, key, watch, remove, threads) = parseCommandLine()
if os.path.exists(inputDir) and os.path.exists(outputDir) and os.path.exists(key):
	print 'inputDir:{}   outputDir:{}   passphrase:{}   watch:{}   remove:{}'.format(
		inputDir, outputDir, passphrase, watch, remove)
	print '==================='
else:
	print 'ERROR: input/output directory or key file does not exist!'
	exit(1)
logging.basicConfig(filename=os.path.join(outputDir, 'fswalk_anonymizer.log'),level=logging.DEBUG)

if threads == None:
	threads = 1

if watch:
	# set up a directory watch
	wm = WatchManager()
	mask = pyinotify.IN_CLOSE_WRITE
	notifier = ThreadedNotifier(wm, FileCloseCb())
	notifier.start()
	wdd = wm.add_watch(inputDir, mask, rec=False)
	# process user input
	run = True
	print 'Type "QUIT" to exit'
	while (run):
		command = raw_input("> ")
		if command == 'QUIT':
			run = False
	notifier.stop()
else:
	# anonymize all files in the inputDir
	for f in os.listdir(inputDir):
		if f.endswith(".ds"):
			while threading.active_count() > threads:
				time.sleep(5)
			try:
				print f
				#anonymizeFile(inputDir, f)
				t = FS_Walk_Anonymizer(inputDir, f)
				t.start()
			except:
				logging.debug("ERROR: [{}] Unable to start the anonymization thread for {}".format(time.strftime("%H:%M:%S %m:%d:%Y", time.localtime()), os.path.join(inputDir, f)))

while threading.active_count() > 1:
	time.sleep(5)
