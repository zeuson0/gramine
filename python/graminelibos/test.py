
import io
import os
import platform
import subprocess
import sys

from . import ninja_syntax

import toml

class TestConfig:
    def __init__(self, path):
        data = toml.load(path)
        self.manifests = []
        if 'manifests' in data:
            self.manifests += data['manifests']

        arch = platform.processor()
        if arch in data and 'manifests' in data[arch]:
            self.manifests += data[arch]['manifests']

        self.sgx_manifests = []
        if 'sgx' in data and 'sgx' in data[arch]:
            self.sgx_manifests = data['sgx']['manifests']

        self.all_manifests = self.manifests + self.sgx_manifests

        self.arch_libdir = '/lib/x86_64-linux-gnu/'  # TODO

        toplevel = subprocess.check_output(['git', 'rev-parse', '--show-toplevel']).decode().strip()
        self.key = os.path.join(toplevel, 'Pal/src/host/Linux-SGX/signer/enclave-key.pem')

    def gen_build_file(self, path):
        print(f'generating {path}')

        output = io.StringIO()
        ninja = ninja_syntax.Writer(output)

        self._gen_header(ninja)
        self._gen_rules(ninja)
        self._gen_targets(ninja)

        with open(path, 'w') as f:
            f.write(output.getvalue())

    def _gen_header(self, ninja):
        ninja.comment('Auto-generated, do not edit!')
        ninja.newline()

    def _gen_rules(self, ninja):
        ninja.variable('ARCH_LIBDIR', self.arch_libdir)
        ninja.variable('KEY', self.key)
        ninja.newline()

        ninja.rule(
            name='manifest',
            command='gramine-manifest -Darch_libdir=$ARCH_LIBDIR -Dentrypoint=$ENTRYPOINT $in $out',
            description='manifest: $out'
        )
        ninja.newline()

        ninja.rule(
            name='sgx-sign',
            command='gramine-sgx-sign --manifest $in --key $KEY --depfile $out.d --output $out >/dev/null',
            depfile='$out.d',
            description='SGX sign: $out',
        )
        ninja.newline()

        ninja.rule(
            name='sgx-get-token',
            command='gramine-sgx-get-token --sig $in --output $out',
            description='SGX token: $out',
        )
        ninja.newline()

    def _gen_targets(self, ninja):
        ninja.build(
            outputs=['direct'],
            rule='phony',
            inputs=[f'{name}.manifest' for name in self.manifests],
        )
        ninja.default('direct')

        ninja.build(
            outputs=['sgx'],
            rule='phony',
            inputs=([f'{name}.manifest' for name in self.all_manifests] +
                    [f'{name}.manifest.sgx' for name in self.all_manifests] +
                    [f'{name}.sig' for name in self.all_manifests] +
                    [f'{name}.token' for name in self.all_manifests]),
        )

        for name in self.all_manifests:
            template = f'{name}.manifest.template'
            if not os.path.exists(template):
                template = 'manifest.template'

            ninja.build(
                outputs=[f'{name}.manifest'],
                rule='manifest',
                inputs=[template],
                variables={'ENTRYPOINT': name},
            )

            ninja.build(
                outputs=[f'{name}.manifest.sgx'],
                implicit_outputs=[f'{name}.sig'],
                rule='sgx-sign',
                inputs=[f'{name}.manifest'],
                implicit=[self.key],
            )

            ninja.build(
                outputs=[f'{name}.token'],
                rule='sgx-get-token',
                inputs=[f'{name}.sig'],
            )

            ninja.build(
                outputs=[f'direct-{name}'],
                rule='phony',
                inputs=[f'{name}.manifest'],
            )

            ninja.build(
                outputs=[f'sgx-{name}'],
                rule='phony',
                inputs=[f'{name}.manifest', f'{name}.manifest.sgx', f'{name}.sig', f'{name}.token'],
            )

            ninja.newline()


def gen_build_file(force=False):
    path = 'build.ninja'
    if not force and os.path.exists(path):
        return
    config = TestConfig('tests.toml')
    config.gen_build_file(path)


def exec_pytest(sgx, args):
    env = os.environ.copy()
    env['SGX'] = '1' if sgx else ''

    argv = [os.path.basename(sys.executable), '-m', 'pytest'] + list(args)
    print(' '.join(argv))
    os.execve(sys.executable, argv, env)


def run_ninja_or_exit(args):
    argv = ['ninja'] + list(args)
    print(' '.join(argv))
    p = subprocess.run(argv)
    if p.returncode != 0:
        sys.exit(p.returncode)


def exec_ninja(args):
    argv = ['ninja'] + list(args)
    print(' '.join(argv))
    os.execvp('ninja', argv)
