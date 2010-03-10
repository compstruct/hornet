all:

check:
if ENABLE_TESTSUITE
	$(QMTEST) run
else
	echo 'ERROR: testsuite disabled at configure stage'
	exit 1
endif
