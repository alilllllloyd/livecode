{
	'variables':
	{
		'app_bundle_suffix': '',
		'ext_bundle_suffix': '.so',
		'lib_suffix': '.so',
		'ext_suffix': '.so',
		'exe_suffix': '',
		'debug_info_suffix': '.dbg',
		
		'c++_std': '<!(echo ${CXX_STD:-c++03})',

		'supports_lto': "<!(echo 'main(){}' | if ${CC:-cc} -flto -fuse-ld=gold -Wl,--sort-section=none -o /dev/null -x c++ - 2>/dev/null >/dev/null; then echo 1; else echo 0; fi)",
	},
	
	'defines':
	[
		'HAVE___THREAD',
		'_FILE_OFFSET_BITS=64',			
	],
	
	# We supply some pre-packaged headers for Linux libraries
	'include_dirs':
	[
		'../thirdparty/headers/linux/include',
		'../thirdparty/libcairo/src',			# Required by the GDK headers
		'../thirdparty/libfreetype/include',	# Required by the Pango headers
	],

	'conditions':
	[
		[
			# For consistency, we should use the same linker in both Debug and Release builds
			'supports_lto != 0',
			{
				'ldflags':
				[
					'-fuse-ld=gold',
				],
			},
		],
	],
	
	# Static libraries that are to be included into dynamic libraries
	# need to be compiled with the correct compilation flags
	'target_conditions':
	[
		[
			'_type == "loadable_module" or _type == "shared_library" or (_type == "static_library" and library_for_module != 0)',
			{
				'cflags':
				[
					'-fPIC',
				],
			},
		],
		[
			'server_mode == 0',
			{
				'defines':
				[
					'TARGET_PLATFORM_LINUX',
					'TARGET_PLATFORM_POSIX',
					'GTKTHEME',
					'LINUX',
					'_LINUX',
					'X11',
				],
			},
			{
				'defines':
				[
					'_LINUX',
					'_SERVER',
					'_LINUX_SERVER',
				],
			},
		],
		[
			'silence_warnings == 0',
			{
				'cflags':
				[
					'-Wall',
					'-Wextra',
					'-Wno-unused-parameter',	# Just contributes build noise
					'-Werror=return-type',
				],
			},
			{
				'cflags':
				[
					'-w',						# Disable warnings
					'-fpermissive',				# Be more lax with old code
					'-Wno-return-type',
				],
				
				'cflags_c':
				[
					'-Werror=declaration-after-statement',	# Ensure compliance with C89
				],
			},
		],
	],
	
	'cflags':
	[
		'-fstrict-aliasing',
		'-fvisibility=hidden',
	],
	
	'cflags_c':
	[
		'-std=gnu99',
		'-Wstrict-prototypes',
	],
	
	'cflags_cc':
	[
		'-std=<(c++_std)',
		'-fno-exceptions',
		'-fno-rtti',
		'-fcheck-new',
	],
	
	'configurations':
	{
		'Debug':
		{
			'cflags':
			[
				'-O0',
				'-g3',
				'-Werror=uninitialized',
			],
			
			'defines':
			[
				'_DEBUG',
			],
		},
		
		'Release':
		{
			'cflags':
			[
				'-O3',
				'-g3',
			],

			'conditions':
			[
				[
					'supports_lto != 0',
					{
						'cflags':
						[
							'-flto',
							'-ffunction-sections',
						],

						'ldflags':
						[
							'-flto',
							'-Wl,--icf=all',
							'>@(_cflags)',
							'>@(_cflags_cc)',
						],
					},
				],
			],
			
			'defines':
			[
				'_RELEASE',
				'NDEBUG',
			],
		},
		
		'Fast':
		{
			'cflags':
			[
				'-O0',
				'-g0',
				'-Werror=uninitialized',
			],
			
			'defines':
			[
				'_RELEASE',
				'NDEBUG',
			],
		},
	},
}
