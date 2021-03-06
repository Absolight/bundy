# Copyright (C) 2013  Internet Systems Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SYSTEMS CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SYSTEMS CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import unittest
import os
import socket
import time
import select
import threading

import bundy.log
from bundy.dns import *
import bundy.datasrc
from bundy.memmgr.builder import *
from bundy.server_common.datasrc_clients_mgr import DataSrcClientsMgr
from bundy.memmgr.datasrc_info import *

TESTDATA_PATH = os.environ['TESTDATA_PATH'] + os.sep

# Defined for easier tests with DataSrcClientsMgr.reconfigure(), which
# only needs get_value() method
class MockConfigData:
    def __init__(self, data):
        self.__data = data

    def get_value(self, identifier):
        return self.__data[identifier], False

class TestMemorySegmentBuilder(unittest.TestCase):
    def _create_builder_thread(self):
        (self._master_sock, self._builder_sock) = \
            socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)

        self._builder_command_queue = []
        self._builder_response_queue = []

        self._builder_lock = threading.Lock()
        self._builder_cv = threading.Condition(lock=self._builder_lock)

        self._builder = MemorySegmentBuilder(self._builder_sock,
                                             self._builder_cv,
                                             self._builder_command_queue,
                                             self._builder_response_queue)
        self._builder_thread = threading.Thread(target=self._builder.run)

    def setUp(self):
        self._create_builder_thread()
        self.__mapped_file_path = None

    def tearDown(self):
        # It's the tests' responsibility to stop and join the builder
        # thread if they start it.
        self.assertFalse(self._builder_thread.isAlive())

        self._master_sock.close()
        self._builder_sock.close()

        if self.__mapped_file_path is not None:
            if os.path.exists(self.__mapped_file_path):
                os.unlink(self.__mapped_file_path)

    # Helper for synchronizing with the builder thread: wait until all commands
    # are completed, and send shutdown command.  Don't wait too long
    # in case of unexpected hangup.
    def __wait_and_shutdown(self):
        wait_limit = 5          # seconds
        while self._builder_command_queue:
            time.sleep(0.5)
            wait_limit -= 0.5
        with self._builder_cv:
            self._builder_command_queue.append(('shutdown',))
            self._builder_cv.notify_all()
        # Confirm it didn't time out.
        self.assertTrue(wait_limit >= 0)

    def test_bad_command(self):
        """Tests what happens when a bad command is passed to the
        MemorySegmentBuilder.
        """

        self._builder_thread.start()

        # Now that the builder thread is running, send it a bad
        # command. The thread should exit its main loop and be joinable.
        with self._builder_cv:
            self._builder_command_queue.append(('bad_command',))
            self._builder_cv.notify_all()

        # Wait 5 seconds to receive a notification on the socket from
        # the builder.
        (reads, _, _) = select.select([self._master_sock], [], [], 5)
        self.assertTrue(self._master_sock in reads)

        # Reading 1 byte should not block us here, especially as the
        # socket is ready to read. It's a hack, but this is just a
        # testcase.
        got = self._master_sock.recv(1)
        self.assertEqual(got, b'x')

        # Wait 5 seconds at most for the main loop of the builder to
        # exit.
        self._builder_thread.join(5)
        self.assertFalse(self._builder_thread.isAlive())

        # The command queue must be cleared, and the response queue must
        # contain a response that a bad command was sent. The thread is
        # no longer running, so we can use the queues without a lock.
        self.assertEqual(len(self._builder_command_queue), 0)
        self.assertEqual(len(self._builder_response_queue), 1)

        response = self._builder_response_queue[0]
        self.assertTrue(isinstance(response, tuple))
        self.assertTupleEqual(response, ('bad_command',))
        del self._builder_response_queue[:]

    def test_shutdown(self):
        """Tests that shutdown command exits the MemorySegmentBuilder
        loop.
        """

        self._builder_thread.start()

        # Now that the builder thread is running, send it the "shutdown"
        # command. The thread should exit its main loop and be joinable.
        with self._builder_cv:
            # Commands before 'shutdown' must be ignore, once 'shutdown' is
            # recognized.
            self._builder_command_queue.append(('bad_command_0',))
            self._builder_command_queue.append(('shutdown',))
            # Commands after 'shutdown' must also be ignored.
            self._builder_command_queue.append(('bad_command_1',))
            self._builder_command_queue.append(('bad_command_2',))
            self._builder_cv.notify_all()

        # Wait 5 seconds at most for the main loop of the builder to
        # exit.
        self._builder_thread.join(5)
        self.assertFalse(self._builder_thread.isAlive())

        # The command queue must be cleared, and the response queue must
        # be untouched (we don't use it in this test). The thread is no
        # longer running, so we can use the queues without a lock.
        self.assertEqual(len(self._builder_command_queue), 0)
        self.assertEqual(len(self._builder_response_queue), 0)

    def test_cancel(self):
        """Check response of cancel command.

        For checking the main task of command we don't need a thread; so we
        simply check it returns the expected response.

        """
        # Most of the setup is the same as test_shutdown.  We append the
        # shutdown command to terminate the builder thread.
        self._builder_thread.start()
        with self._builder_cv:
            self._builder_command_queue.append(('cancel', 'dummy arg'))
            self._builder_cv.notify_all()
        self.__wait_and_shutdown()
        self._builder_thread.join(5)
        self.assertFalse(self._builder_thread.isAlive())
        self.assertEqual([('cancel-completed', 'dummy arg')],
                         self._builder_response_queue)

    @unittest.skipIf(os.environ['HAVE_SHARED_MEMORY'] != 'yes',
                     'shared memory is not available')
    def test_load(self):
        """
        Test "load" command.
        """

        mapped_file_dir = os.environ['TESTDATA_WRITE_PATH']
        mgr_config = {'mapped_file_dir': mapped_file_dir}

        cfg_data = MockConfigData(
            {"_generation_id": 1,
             "classes":
                 {"IN": [{"type": "MasterFiles",
                          "params": { "example.com":
                                      TESTDATA_PATH + "example.com.zone" },
                          "cache-enable": True,
                          "cache-type": "mapped"}]
                  }
             })
        cmgr = DataSrcClientsMgr(use_cache=True)
        cmgr.reconfigure({}, cfg_data)

        genid, clients_map = cmgr.get_clients_map()
        datasrc_info = DataSrcInfo(genid, clients_map, mgr_config)

        self.assertEqual(1, datasrc_info.gen_id)
        self.assertEqual(clients_map, datasrc_info.clients_map)
        self.assertEqual(1, len(datasrc_info.segment_info_map))
        sgmt_info = datasrc_info.segment_info_map[(RRClass.IN, 'MasterFiles')]
        self.assertIsNotNone(sgmt_info.get_reset_param(SegmentInfo.READER))
        self.assertIsNotNone(sgmt_info.get_reset_param(SegmentInfo.WRITER))

        param = sgmt_info.get_reset_param(SegmentInfo.WRITER)
        self.__mapped_file_path = param['mapped-file']

        self._builder_thread.start()

        # Now that the builder thread is running, send it the "load"
        # command. We should be notified when the load operation is
        # complete.
        with self._builder_cv:
            self._builder_command_queue.append(('load',
                                                bundy.dns.Name("example.com"),
                                                datasrc_info, RRClass.IN,
                                                'MasterFiles'))
            self._builder_cv.notify_all()

        # Wait 60 seconds to receive a notification on the socket from
        # the builder.
        (reads, _, _) = select.select([self._master_sock], [], [], 60)
        self.assertTrue(self._master_sock in reads)

        # Reading 1 byte should not block us here, especially as the
        # socket is ready to read. It's a hack, but this is just a
        # testcase.
        got = self._master_sock.recv(1)
        self.assertEqual(got, b'x')

        with self._builder_lock:
            # The command queue must be cleared, and the response queue
            # must contain a response that a bad command was sent. The
            # thread is no longer running, so we can use the queues
            # without a lock.
            self.assertEqual(len(self._builder_command_queue), 0)
            self.assertEqual(len(self._builder_response_queue), 1)

            response = self._builder_response_queue[0]
            self.assertTrue(isinstance(response, tuple))
            self.assertTupleEqual(response, ('load-completed', datasrc_info,
                                             RRClass.IN, 'MasterFiles', True))
            del self._builder_response_queue[:]

        # Now try looking for some loaded data
        clist = datasrc_info.clients_map[RRClass.IN]
        dsrc, finder, exact = clist.find(bundy.dns.Name("example.com"))
        self.assertIsNotNone(dsrc)
        self.assertTrue(isinstance(dsrc, bundy.datasrc.DataSourceClient))
        self.assertIsNotNone(finder)
        self.assertTrue(isinstance(finder, bundy.datasrc.ZoneFinder))
        self.assertTrue(exact)

        # Send the builder thread the "shutdown" command. The thread
        # should exit its main loop and be joinable.
        with self._builder_cv:
            self._builder_command_queue.append(('shutdown',))
            self._builder_cv.notify_all()

        # Wait 5 seconds at most for the main loop of the builder to
        # exit.
        self._builder_thread.join(5)
        self.assertFalse(self._builder_thread.isAlive())

        # The command queue must be cleared, and the response queue must
        # be untouched (we don't use it in this test). The thread is no
        # longer running, so we can use the queues without a lock.
        self.assertEqual(len(self._builder_command_queue), 0)
        self.assertEqual(len(self._builder_response_queue), 0)

    def test_validate(self):
        def raise_ex():
            raise ValueError('test error')
        action_true = lambda: True
        action_false = lambda: False
        action_raise = lambda: raise_ex()

        self._builder_thread.start()

        # issue two validate commands and then stop the thread
        with self._builder_cv:
            self._builder_command_queue.append(('validate', 'd', 'c', 'n',
                                                action_true))
            self._builder_command_queue.append(('validate', 'd', 'c', 'n',
                                                action_false))
            # if the action raises an exception, it'll be caught and responded
            # as a validation failure
            self._builder_command_queue.append(('validate', 'd', 'c', 'n',
                                                action_raise))
            self._builder_cv.notify_all()
        self.__wait_and_shutdown()
        self._builder_thread.join(5)
        self.assertFalse(self._builder_thread.isAlive())

        # Confirm the responses
        self.assertEqual(len(self._builder_response_queue), 3)
        self.assertEqual(('validate-completed', 'd', 'c', 'n', True),
                         self._builder_response_queue[0])
        self.assertEqual(('validate-completed', 'd', 'c', 'n', False),
                         self._builder_response_queue[1])
        self.assertEqual(('validate-completed', 'd', 'c', 'n', False),
                         self._builder_response_queue[2])

# Exercise some detailed cases without bothering to handle threads
class TestMemorySegmentBuilderWithoutThread(unittest.TestCase):
    def setUp(self):
        self.__reset_ok = True
        self.__create_ok = False
        self.__get_cached_zone_writer_ok = True
        self.__load_ok = True
        self.__load_twice = False
        self.__install_ok = True
        self.__installed = False
        self.__response_queue = []
        self.__load_arg = []
        self.__zone_table_result = [] # for get_zone_table_accessor mock
        builder_cv = threading.Condition(lock=threading.Lock())
        self.__builder = MemorySegmentBuilder(self, builder_cv, [],
                                              self.__response_queue)
        self.clients_map = {RRClass.IN: self} # pretend to be client list
        self.dsrc_info = self
        self.segment_info_map = \
            {(RRClass.IN, 'testsrc'): self} # pretend to be segment info

    def send(self, x):          # mock socket.send()
        return 1

    # Mock SegmentInfo.get_reset_param().
    def get_reset_param(self, unused):
        return {}

    # Mock ConfigurableClientList.get_cached_zone_writer()
    def get_cached_zone_writer(self, name, catch_error, dsrc_name):
        if self.__get_cached_zone_writer_ok:
            return (ConfigurableClientList.CACHE_STATUS_ZONE_SUCCESS, self)
        raise bundy.datasrc.Error('test')

    # Mock ConfigurableClientList.reset_memory_segment()
    def reset_memory_segment(self, dsrc_name, reset_type, param):
        self.__reset_called += 1 # for inspection

        # for now, we always make read-only reset succeed
        if reset_type == ConfigurableClientList.READ_ONLY:
            return

        if self.__reset_ok:     # OK: configured to succeed
            return

        # if it's a create attempt and is configured so, make it succeed.
        if self.__create_ok and reset_type == ConfigurableClientList.CREATE:
            return

        # Otherwise, we'll make it fail with an exceptin (type doesn't matter)
        raise ValueError('test')

    # Mock ConfigurableClientList.get_zone_table_accessor()
    def get_zone_table_accessor(self, arg1, arg2):
        return self.__zone_table_result

    def load(self, arg):             # mock ZoneWriter.load()
        self.__load_arg.append(arg)
        if not self.__load_ok:
            raise ValueError('load fail')
        if self.__load_twice and len(self.__load_arg) == 1:
            return False
        return True

    def install(self):          # mock ZoneWriter.install()
        if not self.__install_ok:
            raise ValueError('install fail')
        self.__installed = True

    def cleanup(self):          # Mock ZoneWriter.cleanup
        self.__writer_cleanup_called = True

    def __check_reset_fail(self, name, create_ok):
        self.__reset_called = 0
        del self.__response_queue[:]
        self.__create_ok = create_ok

        # reset in the rw mode will fai, falling back to creating a new one.
        # if 'create_ok', it succeeds, and reset_segment() will be called 3
        # times in tatal, including the last one for read-only.  response
        # queue should have a success result.
        # if not 'create_ok', even the create attempt will fail, so the failure
        # will be reported in the response queue, the last reset attempt won't
        # happen.
        self.__builder._handle_load(name, self, RRClass.IN, 'testsrc')
        self.assertEqual(self.__response_queue[0],
                         ('load-completed', self, RRClass.IN, 'testsrc',
                          create_ok))
        self.assertEqual(self.__reset_called, 3 if create_ok else 2)

    def test_reset_fail(self):
        self.__reset_ok = False
        self.__check_reset_fail(Name('test.example'), True)
        self.__check_reset_fail(Name('test.example'), False)
        self.__check_reset_fail(None, True)
        self.__check_reset_fail(None, False)

    def __check_load(self, twice):
        """Common test case for successful load.

        If 'twice' is True, load() will have to called twice to complete load.

        """
        # set up
        self.__writer_cleanup_called = 0
        self.__reset_called = 0
        self.__load_arg = []
        del self.__response_queue[:]
        self.__load_twice = twice

        # Do the load
        self.__builder._handle_load(Name('test.example'), self, RRClass.IN,
                                    'testsrc')

        # Check what happened
        self.assertEqual([1000, 1000] if twice else [1000], self.__load_arg)
        self.assertTrue(self.__installed)
        self.assertEqual(1, self.__writer_cleanup_called)
        self.assertEqual(2, self.__reset_called) # reset for rw and ro
        self.assertEqual(self.__response_queue[0],
                         ('load-completed', self, RRClass.IN, 'testsrc', True))

    def test_load(self):
        self.__check_load(False)
        self.__check_load(True)

    def __check_load_fail(self, name, writer_ok, load_ok, install_ok):
        self.__reset_called = 0
        del self.__response_queue[:]
        self.__get_cached_zone_writer_ok = writer_ok
        self.__load_ok = load_ok
        self.__install_ok = install_ok
        self.__builder._handle_load(name, self, RRClass.IN, 'testsrc')

        # if any of the above fails, loading a specific zone is considered
        # a failure; full load is considered to be succeeded as long as load
        # was attmpted.
        expect_success = name is None
        self.assertEqual(self.__response_queue[0],
                         ('load-completed', self, RRClass.IN, 'testsrc',
                          expect_success))

    def test_load_fail(self):
        # To load a specific load, any intermediate error makes load fail
        self.__check_load_fail(Name('test.example'), False, None, None)
        self.__check_load_fail(Name('test.example'), True, False, None)
        self.__check_load_fail(Name('test.example'), True, True, False)

        # for full load, it's considered a success as long as load is tried.
        self.__zone_table_result = [(None, Name('test.example'))]
        self.__check_load_fail(None, False, None, None)
        self.__check_load_fail(None, True, False, None)
        self.__check_load_fail(None, True, True, False)

    def __check_load_cancel(self, reason, expect_cancel=True):
        "Common check for cancel/shutdown interrupt during a load."

        # setup
        self.__writer_cleanup_called = 0
        self.__reset_called = 0
        self.__load_arg = []
        del self.__response_queue[:]
        self.__builder._command_queue = [('dummy', 1), reason]
        self.gen_id = 42        # prentending to be dsrc_info for log message

        # do the load
        self.__builder._handle_load(Name('test.example'), self, RRClass.IN,
                                    'testsrc')

        # In call cases, graceful cleanup is performed.
        self.assertEqual(1, self.__writer_cleanup_called)
        self.assertEqual(2, self.__reset_called) # still reset for rw and ro

        if expect_cancel:
            self.assertEqual([1000], self.__load_arg) # only one call to load()
            self.assertFalse(self.__installed) # not installed due to the cancel
            self.assertFalse(self.__response_queue) # no response
        else:
            # if it's not canceled, the result is like a normal load
            self.assertEqual([1000, 1000] if self.__load_twice else [1000],
                             self.__load_arg)
            self.assertTrue(self.__installed)
            self.assertEqual(self.__response_queue[0],
                             ('load-completed', self, RRClass.IN, 'testsrc',
                              True))

    def test_load_cancel(self):
        # Possible can only happen if the load() isn't completed in the first
        # call, so we should generally set up so the second load is required.
        self.__load_twice = True
        self.__check_load_cancel(('shutdown',))
        self.__check_load_cancel(('cancel', self))
        self.__check_load_cancel(('cancel', 'dummy-src'), False)

        # If the first call to load() completes its task, no cancel happens.
        self.__load_twice = False
        self.__check_load_cancel(('shutdown',), False)

    def test_handle_cancels(self):
        "Check the effect of cancel commands."

        # The result of _handle_cancels consists of the input list elements
        # that don't match cancel commands.  cancel commands themselves remain.
        commands = [('validate', 'info1'), ('load', 0, 'info2'),
                    ('validate', 'info3'),
                    ('cancel', 'info1'), ('cancel', 'info2'),
                    ('validate', 'info2'), ('load', 1, 'info1'),
                    ('load', 2, 'info3')]
        result = self.__builder._handle_cancels(commands)
        self.assertEqual([('validate', 'info3'), ('cancel', 'info1'),
                          ('cancel', 'info2'), ('load', 2, 'info3')], result)

        commands.extend([('shutdown',), ('load', 3, 'info4')])
        result = self.__builder._handle_cancels(commands)
        self.assertEqual([('shutdown',)], result)

if __name__ == "__main__":
    bundy.log.init("bundy-test")
    bundy.log.resetUnitTestRootLogger()
    unittest.main()
