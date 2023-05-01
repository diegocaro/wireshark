# Copyright (c) 2013 by Gilbert Ramirez <gram@alumni.rice.edu>
#
# SPDX-License-Identifier: GPL-2.0-or-later

import subprocess
import pytest


@pytest.fixture
def dfilter_cmd(cmd_tshark, capture_file, request):
    def wrapped(dfilter, frame_number=None, prefs=None, read_filter=False):
        cmd = [
            cmd_tshark,
            "-n",       # No name resolution
            "-r",       # Next arg is trace file to read
            capture_file(request.instance.trace_file),
        ]
        if frame_number:
            cmd.extend([
                "-2",       # two-pass mode
                "--selected-frame={}".format(frame_number)
            ])
        if read_filter:
            cmd.extend([
                "-2",       # two-pass mode
                "-R",       # read filter (requires two-pass mode)
                dfilter
            ])
        else:
            cmd.extend([
                "-Y",       # packet display filter (used to be -R)
                dfilter
            ])
        if prefs:
            cmd.extend([
            "-o",
            prefs
        ])
        return cmd
    return wrapped

@pytest.fixture(scope='session')
def cmd_dftest(program):
    return program('dftest')


@pytest.fixture
def checkDFilterCount(dfilter_cmd, base_env):
    def checkDFilterCount_real(dfilter, expected_count, prefs=None):
        """Run a display filter and expect a certain number of packets."""
        output = subprocess.check_output(dfilter_cmd(dfilter, prefs=prefs),
                                         universal_newlines=True,
                                         stderr=subprocess.STDOUT,
                                         env=base_env)

        dfp_count = output.count("\n")
        msg = "Expected %d, got: %s\noutput: %r" % \
            (expected_count, dfp_count, output)
        assert dfp_count == expected_count, msg
    return checkDFilterCount_real

@pytest.fixture
def checkDFilterCountWithSelectedFrame(dfilter_cmd, base_env):
    def checkDFilterCount_real(dfilter, expected_count, selected_frame, prefs=None):
        """Run a display filter and expect a certain number of packets."""
        output = subprocess.check_output(dfilter_cmd(dfilter, frame_number=selected_frame, prefs=prefs),
                                         universal_newlines=True,
                                         stderr=subprocess.STDOUT,
                                         env=base_env)

        dfp_count = output.count("\n")
        msg = "Expected %d, got: %s\noutput: %r" % \
            (expected_count, dfp_count, output)
        assert dfp_count == expected_count, msg
    return checkDFilterCount_real

@pytest.fixture
def checkDFilterCountReadFilter(dfilter_cmd, base_env):
    def checkDFilterCount_real(dfilter, expected_count):
        """Run a read filter in two pass mode and expect a certain number of packets."""
        output = subprocess.check_output(dfilter_cmd(dfilter, read_filter=True),
                                         universal_newlines=True,
                                         stderr=subprocess.STDOUT,
                                         env=base_env)

        dfp_count = output.count("\n")
        msg = "Expected %d, got: %s\noutput: %r" % \
            (expected_count, dfp_count, output)
        assert dfp_count == expected_count, msg
    return checkDFilterCount_real

@pytest.fixture
def checkDFilterFail(cmd_dftest, base_env):
    def checkDFilterFail_real(dfilter, error_message):
        """Run a display filter and expect dftest to fail."""
        proc = subprocess.Popen([cmd_dftest, '--', dfilter],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                universal_newlines=True,
                                env=base_env)
        outs, errs = proc.communicate()
        assert error_message in errs, \
            'Unexpected dftest stderr:\n%s\nstdout:\n%s' % (errs, outs)
        assert proc.returncode == 4, \
            'Unexpected dftest exit code: %d. stdout:\n%s\n' % \
            (proc.returncode, outs)
    return checkDFilterFail_real

@pytest.fixture
def checkDFilterSucceed(cmd_dftest, base_env):
    def checkDFilterSucceed_real(dfilter, expect_stdout=None):
        """Run a display filter and expect dftest to succeed."""
        proc = subprocess.Popen([cmd_dftest, '--', dfilter],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                universal_newlines=True,
                                env=base_env)
        outs, errs = proc.communicate()
        assert proc.returncode == 0, \
            'Unexpected dftest exit code: %d. stderr:\n%s\n' % \
            (proc.returncode, errs)
        if expect_stdout:
            assert expect_stdout in outs, \
                'Expected the string %s in the output' % expect_stdout
    return checkDFilterSucceed_real
