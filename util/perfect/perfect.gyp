{
	'includes':
	[
		'../../common.gypi',
	],
	
	'targets':
	[
		{
			'target_name': 'perfect',
			'type': 'executable',
			
			'sources':
			[
				'perfect.c',
			],
			
			'msvs_settings':
			{
				'VCLinkerTool':
				{
					'SubSystem': '1',	# /SUBSYSTEM:CONSOLE
				},
			},
			
			'direct_dependent_settings':
			{
				'variables':
				{
					'perfect_path': '<(PRODUCT_DIR)/perfect<(EXECUTABLE_SUFFIX)',
				},
			},
		},
	],
}
