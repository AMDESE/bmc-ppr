project(
  'amd-bmc-ppr',
  'cpp',
  default_options: [
    'warning_level=3',
    'werror=true',
    'cpp_std=c++20'
  ],
  license: 'Apache-2.0',
  version: '1.0',
)


cpp = meson.get_compiler('cpp')

if (get_option('bmc_dev_irq').enabled())
  add_project_arguments('-DBMC_DEV_IRQ', language:'cpp')
endif

# RT PPR file-watcher paths
rt_ppr_watch_dir  = get_option('ras_watch_dir')
rt_ppr_src_config = get_option('rt_ppr_src_config_file')
rt_ppr_config     = get_option('rt_ppr_config_file')

cpp_args = [
  '-DRAS_WATCH_DIR="'      + rt_ppr_watch_dir  + '"',
  '-DCONFIG_FILE="'        + rt_ppr_config     + '"',
  '-DSRC_CONFIG_FILE="'    + rt_ppr_src_config + '"',
  '-DRT_PPR_CONFIG_FILE="' + rt_ppr_config     + '"',
]

deps = [
  dependency('libgpiodcxx', default_options: ['bindings=cxx']),
  dependency('systemd'),
  dependency('sdbusplus'),
  dependency('sdeventplus'),
  dependency('phosphor-logging'),
  dependency('nlohmann_json'),
  dependency('threads'),
  cpp.find_library('apml64'),
]

executable(
  'amd-bmc-ppr',
  'src/main.cpp',
  'src/rt_ppr.cpp',
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

install_data(
  ['config/sppr_config.json'],
  install_dir: get_option('datadir') / 'amd-bmc-ppr'
)
