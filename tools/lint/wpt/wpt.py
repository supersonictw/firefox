# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
import platform
import sys

from mozprocess import ProcessHandler

from mozlint import result

results = []


def lint(files, config, **kwargs):
    tests_dir = os.path.join(kwargs['root'], 'testing', 'web-platform', 'tests')

    def process_line(line):
        try:
            data = json.loads(line)
        except ValueError:
            return

        data["level"] = "error"
        data["path"] = os.path.relpath(os.path.join(tests_dir, data["path"]), kwargs['root'])
        data.setdefault("lineno", 0)
        results.append(result.from_config(config, **data))

    if files == [tests_dir]:
        print("No specific files specified, running the full wpt lint"
              " (this is slow)", file=sys.stderr)
        files = ["--all"]
    cmd = [os.path.join(tests_dir, 'wpt'), 'lint', '--json'] + files
    if platform.system() == 'Windows':
        cmd.insert(0, sys.executable)

    proc = ProcessHandler(cmd, env=os.environ, processOutputLine=process_line)
    proc.run()
    try:
        proc.wait()
        if proc.returncode != 0:
            results.append(
                result.from_config(config,
                                   message="Lint process exited with return code %s" %
                                   proc.returncode))
    except KeyboardInterrupt:
        proc.kill()

    return results
