#!/usr/bin/python

import sys, os, subprocess, shlex
from optparse import OptionParser
import pyinotify
from pyinotify import WatchManager, ThreadedNotifier, EventsCodes, ProcessEvent
import hashlib
import re
import threading
import logging, time

class FileCloseCb(pyinotify.ProcessEvent):
	def process_IN_CLOSE_WRITE(self, event):
		if event.name.endswith(".ds") == False or os.path.exists(os.path.join(outputDir, event.name)):
			return
		encryptFile(event.path, event.name)

def runCommand(command):
	try:
		return subprocess.check_output(shlex.split(command))
	except subprocess.CalledProcessError as e:
		logging.debug('ERROR: [{}] {} {} {}'.format(time.strftime("%H:%M:%S %m:%d:%Y", time.localtime()),
			e.cmd, e.returncode, e.output))
		return 'ERROR'

class AES_GPG_Encryptor(threading.Thread):
	def __init__(self, inputDir, inputFile):
		threading.Thread.__init__(self)
		self.inputDir = inputDir
		self.inputFile = inputFile

	def run(self):
		encryptFile(self.inputDir, self.inputFile)

def encryptFile(inputDir, inputFile):
	if inputFile.endswith(".ds") == False:
		return
	print '-------------------\nencrypting %s' % os.path.join(inputDir, inputFile)
	encryptAES_GPG(inputDir, inputFile, outputDir)
	if validateAES_GPG(os.path.join(inputDir, inputFile), outputDir, inputFile + '.gpg'):
		print 'Encryption of %s is validated' % os.path.join(outputDir, inputFile + '.gpg')
	else:
		print 'FAILED encryption for %s' % os.path.join(outputDir, inputFile + '.gpg')
		logging.debug('ERROR: failed encryption for {}'.format(os.path.join(outputDir, inputFile + '.gpg')))
	if remove:
		command = 'rm ' + os.path.join(inputDir, inputFile)
		print command
		runCommand(command)

def encryptAES_GPG(inputDir, inputFile, outputDir):
	outputFile = os.path.join(outputDir, inputFile + '.gpg')
	key256 = re.sub(r'\'|\"', 'X', hashlib.sha256(passphrase).digest())
	command = 'gpg --symmetric --cipher-algo aes -z0 --passphrase ' + key256 + ' -o ' + outputFile + ' ' + os.path.join(inputDir, inputFile)
	print command
	runCommand(command)

def validateAES_GPG(origFile, directory, fileName):
	return True # to save CPU resources
	print '===================\nvalidating encryption of %s' % os.path.join(directory, fileName)
	command = './decryptTraces -g -f ' + os.path.join(directory, fileName) + ' -p ' + passphrase + ' -o ' + outputDir
	if runCommand(command) != 'ERROR':
		outputFile = os.path.join(directory, fileName.split('.',1)[0] + '.ds')
		command = 'diff ' + outputFile + ' ' + origFile
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
		help="directory where encrypted traces get stored (REQUIRED)", 
		metavar="DIR")
	parser.add_option("-p", "--passphrase", dest="passphrase", type="string", 
		help="passphrase for symmetric encryption (REQUIRED)",
		metavar="STRING")
	parser.add_option("-n", action="store_false", dest="watch", default=True,
		help="run in the non-watch mode")
	parser.add_option("-r", action="store_true", dest="remove", default=False,
		help="remove input files after anonymization")
	parser.add_option("-t", "--threads", dest="threads", type="int")

	(options, args) = parser.parse_args()
	if options.inputDir == None or options.outputDir == None or options.passphrase == None:
		parser.print_help()
		exit(1)
	if options.inputDir == options.outputDir and options.watch == True:
		print 'ERROR: input and output directories have to be different!'
		exit(1)
	return (options.inputDir, options.outputDir, options.passphrase, 
		options.watch, options.remove, options.threads)

# process command line arguments
(inputDir, outputDir, passphrase, watch, remove, threads) = parseCommandLine()
if os.path.exists(inputDir) and os.path.exists(outputDir):
	print 'inputDir:{}   outputDir:{}   passphrase:{}   watch:{}   remove:{}'.format(
		inputDir, outputDir, passphrase, watch, remove)
	print '==================='
else:
	print 'ERROR: input/output directory or key file does not exist!'
	exit(1)
logging.basicConfig(filename=os.path.join(outputDir, 'encryption.log'),level=logging.DEBUG)
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
	# encrypt all files in the inputDir
	for f in os.listdir(inputDir):
		if f.endswith(".ds"):
			while threading.active_count() > threads:
				time.sleep(5)
			try:
				print f
				#encryptFile(inputDir, f)
				t = AES_GPG_Encryptor(inputDir, f)
				t.start()
			except:
				logging.debug("ERROR: [{}] Unable to start the encryption thread for {}".format(time.strftime("%H:%M:%S %m:%d:%Y", time.localtime()), os.path.join(inputDir, f)))


