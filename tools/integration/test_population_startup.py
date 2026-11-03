#!/usr/bin/env python3
"""Compile Population NPCs and load YAML using the production Release engine, without starting a server."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import re
import tempfile

from cpp_test_tools import visual_studio_dev_cmd

ROOT = Path(__file__).resolve().parents[2]
DEFINES = ['WIN32', 'FD_SETSIZE=4096', 'PCRE_SUPPORT', 'LIBCONFIG_STATIC', 'YY_USE_CONST',
           '_CRT_SECURE_NO_DEPRECATE', '_CRT_NONSTDC_NO_DEPRECATE', '_WINSOCK_DEPRECATED_NO_WARNINGS']
INCLUDES = ['src', '3rdparty/libconfig', '3rdparty/rapidyaml/src',
            '3rdparty/rapidyaml/ext/c4core/src', '3rdparty/pcre/include', '3rdparty/mysql/include']
LIBRARIES = ['.vs/build/common.lib', '.vs/build/ryml.lib', '.vs/build/libconfig.lib',
             '3rdparty/zlib/lib/x64/zlib.lib', '3rdparty/pcre/lib/x64/pcre8.lib',
             '3rdparty/mysql/lib/x64/libmysql.lib']
SYSTEM_LIBRARIES = ['ws2_32.lib', 'advapi32.lib', 'user32.lib']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/population-startup-test')
    parser.add_argument('--objects', type=Path, default=ROOT / '.vs/build/map-server/x64/Release')
    parser.add_argument('--mode', choices=['PRE', 'RE'], default='RE',
                        help='Must match the Release objects: PRE uses DefineConstants=PRERE')
    parser.add_argument('--preset', action='store_true', help='Test the optional pre-renewal 4000-shell preset')
    args = parser.parse_args()
    if args.preset and args.mode != 'PRE':
        parser.error('--preset requires --mode PRE')
    if os.name != 'nt':
        print('SKIP: production-object startup fixture requires Windows Release x64 objects')
        return 0
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    objects = sorted(path for path in args.objects.resolve().glob('*.obj')
                     if path.name not in {'script.obj', 'map.obj', 'population_combat.obj'})
    if len(objects) < 50:
        raise RuntimeError('Build src/map/map-server.vcxproj in Release x64 first')
    executable = output / 'test.exe'
    fixture = output / 'fixture.obj'
    compile_flags = ['/nologo', '/c', '/std:c++17', '/EHsc', '/MT', '/O2', '/UNDEBUG', '/bigobj',
                     *['/D' + value for value in DEFINES + (['PRERE'] if args.mode == 'PRE' else [])],
                     *['/I' + str(ROOT / path) for path in INCLUDES],
                     str(ROOT / 'tools/test_population_startup.cpp'), '/Fo:' + str(fixture)]
    map_fixture = output / 'map-fixture.obj'
    map_flags = [*compile_flags[:-2], str(ROOT / 'tools/population_warp_test_maps.cpp'),
                 '/Fo:' + str(map_fixture)]
    combat_fixture = output / 'combat-fixture.obj'
    combat_flags = [*compile_flags[:-2], str(ROOT / 'tools/population_support_test_combat.cpp'),
                    '/Fo:' + str(combat_fixture)]
    link_flags = ['/NOLOGO', '/LTCG', '/SUBSYSTEM:CONSOLE', '/OUT:' + str(executable),
                  str(fixture), str(map_fixture), str(combat_fixture), *[str(path) for path in objects],
                  *[str(ROOT / path) for path in LIBRARIES], *SYSTEM_LIBRARIES]
    for name, flags in [('compile.rsp', compile_flags), ('compile-map.rsp', map_flags),
                        ('compile-combat.rsp', combat_flags), ('link.rsp', link_flags)]:
        (output / name).write_text('\n'.join(subprocess.list2cmdline([flag]) for flag in flags) + '\n')
    script = output / 'build.cmd'
    script.write_text(
        f'@echo off\ncall "{visual_studio_dev_cmd()}" -arch=x64 -host_arch=x64 >nul\n'
        f'if errorlevel 1 exit /b 1\nset CL=\nset _CL_=\ncd /d "{output}"\n'
        f'cl @"{output / "compile.rsp"}"\nif errorlevel 1 exit /b %errorlevel%\n'
        f'cl @"{output / "compile-map.rsp"}"\nif errorlevel 1 exit /b %errorlevel%\n'
        f'cl @"{output / "compile-combat.rsp"}"\nif errorlevel 1 exit /b %errorlevel%\n'
        f'link @"{output / "link.rsp"}"\nexit /b %errorlevel%\n')
    process_flags = subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP
    with (output / 'link.log').open('w', encoding='utf-8') as log:
        build = subprocess.run(['cmd.exe', '/d', '/c', str(script)], stdout=log,
                               stderr=subprocess.STDOUT, timeout=900, creationflags=process_flags)
    if build.returncode:
        raise RuntimeError('Startup fixture build failed; see ' + str(output / 'link.log'))
    if re.search(r'LNK(?:4006|2005)', (output / 'link.log').read_text()):
        raise RuntimeError('Duplicate production definitions; see ' + str(output / 'link.log'))
    for name, vendor in [('libmysql.dll', 'mysql'), ('pcre8.dll', 'pcre'), ('zlib.dll', 'zlib')]:
        source = ROOT / '3rdparty' / vendor / 'lib/x64' / name
        shutil.copy2(source, output / name)
    # Preserve the repository's tracked item imports; fill missing templates in
    # a disposable tree, never in operator configuration. Isolate leak logs too.
    with tempfile.TemporaryDirectory(prefix='population-startup-') as temporary:
        sandbox = Path(temporary)
        shutil.copytree(ROOT / 'db', sandbox / 'db')
        imports = sandbox / 'db/import'
        imports.mkdir(exist_ok=True)
        for template in (ROOT / 'db/import-tmpl').glob('*.yml'):
            if not (imports / template.name).exists():
                shutil.copy2(template, imports / template.name)
        shutil.copytree(ROOT / 'npc/custom/population', sandbox / 'npc/custom/population')
        (sandbox / 'log').mkdir()
        if args.preset:
            for name in ('population_engine.yml', 'population_pvp.yml', 'population_spawn.yml'):
                shutil.copy2(ROOT / 'db/population_presets/prerenewal_4000' / name, sandbox / 'db' / name)
        result = subprocess.run([str(executable), *(['--preset'] if args.preset else [])], cwd=sandbox,
                                capture_output=True, text=True, timeout=120, creationflags=process_flags)
    log = result.stdout + result.stderr
    (output / 'run.log').write_text(log, encoding='utf-8')
    failures = ('[error]', '[fatal error]', 'script error', 'unknown job name',
                'script parse failed', 'index out of range', 'getelementofarray', 'pc_equipitem failed')
    if result.returncode or any(message in log.lower() for message in failures):
        raise RuntimeError('Population startup check failed; see ' + str(output / 'run.log'))
    passed = [line for line in result.stdout.splitlines() if line.startswith('PASS:')]
    if len(passed) != 11:
        raise RuntimeError('Missing startup assertions; see ' + str(output / 'run.log'))
    print('\n'.join(passed))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())