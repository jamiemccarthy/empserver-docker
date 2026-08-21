#!/usr/bin/env python3
"""
empire-probe -- drive an Empire server over the wire, for debugging.

Written from doc/clients-howto.  This is a throwaway probe, not a reference
implementation: the real gateway is Ruby and should be written from the spec,
not translated from here.

    ./empire-probe.py POGO peter 'version' 'xdump table *'
    ./empire-probe.py 1 <password> 'break' 'map -6:6,-4:4 h'

Use '||' to pre-supply answers to C_FLUSH argument prompts:

    ./empire-probe.py POGO peter 'turn off||The game is paused.||.'

Options:
    --host H --port P     default 127.0.0.1:6665
    --raw FILE            also write every received byte to FILE
    --quiet               print only C_DATA text, without the ID column

Never save `xdump nat` output to a file you intend to commit.  Dumped as a
deity, that table includes every country's password in plain text.
"""
import argparse
import socket
import sys

# include/proto.h
ID = {0: 'C_CMDOK', 1: 'C_DATA', 2: 'C_INIT', 3: 'C_EXIT', 4: 'C_FLUSH',
      5: 'C_NOECHO', 6: 'C_PROMPT', 7: 'C_ABORT', 8: 'C_REDIR', 9: 'C_PIPE',
      10: 'C_CMDERR', 11: 'C_BADCMD', 12: 'C_EXECUTE', 13: 'C_FLASH',
      14: 'C_INFORM'}

PROMPTS = {6, 4, 12, 3}          # anything that ends a command's output


class Session:
    def __init__(self, host, port, quiet=False):
        self.sock = socket.create_connection((host, port), timeout=30)
        self.buf = b''
        self.raw = b''
        self.quiet = quiet

    def readline(self):
        """One output line: (id, text-bytes).  None at EOF."""
        while b'\n' not in self.buf:
            data = self.sock.recv(65536)
            if not data:
                return None
            self.raw += data
            self.buf += data
        line, self.buf = self.buf.split(b'\n', 1)
        return int(chr(line[0]), 36), line[2:]     # IDs are base 36

    def send(self, text):
        self.sock.sendall(text.encode() + b'\n')

    def until(self, stop):
        """Read until an ID in `stop`.  Echo as we go."""
        while True:
            got = self.readline()
            if got is None:
                return None, b''
            code, text = got
            shown = text.decode('utf-8', 'replace')
            if self.quiet:
                if code == 1:
                    print(shown)
            else:
                print(f'  {ID.get(code, code):9s} {shown}')
            if code in stop:
                return code, text

    def login(self, country, password):
        self.until({2})                                  # C_INIT prompt
        # options utf-8 MUST precede play: until then the high bit of a text
        # byte means highlighting, and the stream is not valid UTF-8.
        for cmd in ('client empire-probe/1.0', 'options utf-8',
                    f'coun {country}', f'pass {password}'):
            self.send(cmd)
            self.until({0, 10, 11, 3})
        self.send('play')
        code, text = self.until({2, 3, 10})
        if code != 2:
            raise SystemExit('login failed')
        if text != b'2':
            raise SystemExit(f'unknown protocol version {text!r}')
        self.until({6})                                  # first command prompt

    def command(self, spec):
        """Run one command, answering C_FLUSH prompts from `spec`."""
        cmd, *answers = spec.split('||')
        print(f'  -> {cmd}')
        self.send(cmd)
        unanswered = 0
        while True:
            code, text = self.until(PROMPTS)
            if code == 6:                                # C_PROMPT: done
                return True
            if code == 3:                                # C_EXIT
                return False
            if code == 12:                               # C_EXECUTE: batch file
                # The *client* reads the file, so the path is resolved here,
                # not in the server.  A probe on the host cannot read a path
                # inside the container.
                path = text.decode().split()[0]
                try:
                    with open(path) as batch:
                        for line in batch:
                            self.send(line.rstrip('\n'))
                except OSError as err:
                    print(f'  !! cannot read batch file: {err}', file=sys.stderr)
                self.send('ctld')                        # end of batch
            elif code == 4:                              # C_FLUSH: argument
                if answers:
                    self.send(answers.pop(0))
                    unanswered = 0
                    continue
                # A command that keeps prompting will hang a naive client
                # forever.  Give up and interrupt it instead.
                unanswered += 1
                if unanswered > 3:
                    print('  !! unanswered prompts, sending "aborted"',
                          file=sys.stderr)
                    self.send('aborted')
                    unanswered = 0
                else:
                    self.send('')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('country')
    ap.add_argument('password')
    ap.add_argument('commands', nargs='*')
    ap.add_argument('--host', default='127.0.0.1')
    ap.add_argument('--port', type=int, default=6665)
    ap.add_argument('--raw')
    ap.add_argument('--quiet', action='store_true')
    args = ap.parse_args()

    session = Session(args.host, args.port, args.quiet)
    try:
        session.login(args.country, args.password)
        for spec in args.commands:
            if not session.command(spec):
                break
        else:
            session.send('quit')
            session.until({3})
    finally:
        if args.raw:
            with open(args.raw, 'wb') as f:
                f.write(session.raw)
            print(f'  [{len(session.raw)} bytes -> {args.raw}]', file=sys.stderr)


if __name__ == '__main__':
    main()
