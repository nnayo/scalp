define log
	#p LOG.ram_index
	#p LOG.ram_buffer

	set $_i=0
	while ( $_i < LOG.ram_index )
		p  LOG.ram_buffer[$_i].fr.cmde
		printf "time = % 6d ms : status = 0x%02x, t_id = 0x%02x, argv[] = [0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]\n", ((LOG.ram_buffer[$_i].time[0] << 16) + (LOG.ram_buffer[$_i].time[1] << 8)) / 2.56 / 4, LOG.ram_buffer[$_i].fr.status, LOG.ram_buffer[$_i].fr.t_id, LOG.ram_buffer[$_i].fr.argv[0], LOG.ram_buffer[$_i].fr.argv[1], LOG.ram_buffer[$_i].fr.argv[2], LOG.ram_buffer[$_i].fr.argv[3], LOG.ram_buffer[$_i].fr.argv[4], LOG.ram_buffer[$_i].fr.argv[5]
		set $_i++
	end
end

define bsc
	p BSC.in
	p BSC.cont
	p BSC.out
	p BSC.in_fifo
	p BSC.out_fifo
end

define dpt
	p DPT.in
	p DPT.in_fifo
	p DPT.out
	p DPT.out_fifo
	p DPT.appli
	p DPT.appli_fifo
end

#b FIFO_put
#b FIFO_get

#b DPT_run
#b DPT_tx
#b DPT_dispatch

#b BSC_run
#b BSC_frame_handling
#b basic.c:193
#b basic.c:204
#b basic.c:215
#b basic.c:232
#b basic.c:269
#b basic.c:313
#b basic.c:389


#b LOG_log

#b common.c:156
#b common.c:125
#b basic.c:389

b test_free_run_minut

run
fini
log

b test_free_run_bc
continue
fini
log
