.text
	_init:
	mv x0, #0
	ret

	_fn:
	ub _init
	ub _end

	_end:
	call _extern

	_extern:
	beq _end
	ret