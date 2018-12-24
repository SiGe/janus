def Settings( **kwargs ):
  return {
    'flags': ['-O3', '-Wall', '-Werror', '-Iinclude/', '-std=c11', '-fms-extensions', '-Wno-microsoft-anon-tag', '-Ilib/']
  }

