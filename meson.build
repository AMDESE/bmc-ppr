project(
  'amd-bmc-ppr',
  'cpp',
  default_options: [
    'warning_level=3',
    'werror=true',
    'cpp_std=c++17'
  ],
  license: 'Apache-2.0',
  version: '1.0',
)


conf_data = configuration_data()
conf_data.set_quoted('DBUS_OBJECT_NAME', '/xyz/openbmc_project/PostPackageRepair')
conf_data.set_quoted('DBUS_SERVICE_NAME','xyz.openbmc_project.PostPackageRepair')

cpp_args = []

cpp = meson.get_compiler('cpp')

configure_file(output: 'config.h',
  configuration: conf_data
)

# (Meson requires an absolute path for find_library().)
libdir = meson.current_source_dir() + './lib/'

apml_dep = cpp.find_library('apml64', dirs : libdir) # ./lib/libapml64.lib

deps = [
  dependency('libgpiodcxx', default_options: ['bindings=cxx']),
  dependency('systemd'),
  dependency('sdbusplus'),
  dependency('sdeventplus'),
  dependency('phosphor-logging'),
  dependency('nlohmann_json'),
  dependency('threads'),
  apml_dep,
]

executable(
  'amd-bmc-ppr',
  'src/main.cpp',
  'src/rt_ppr.cpp',
  'src/bt_ppr.cpp',
  include_directories: include_directories('inc'),
  cpp_args: cpp_args,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'))

systemd = dependency('systemd')

install_data(
  ['service_files/xyz.openbmc_project.PostPackageRepair.service'],
  install_dir: systemd.get_pkgconfig_variable('systemdsystemunitdir')
)

