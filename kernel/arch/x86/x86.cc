
#include <os.h>

#include <x86.h>
#include <keyboard.h>


extern "C" {


// Return CPUID
regs_t cpu_cpuid(int code)
{
	regs_t r;
	asm volatile("cpuid":"=a"(r.eax),"=b"(r.ebx),
                 "=c"(r.ecx),"=d"(r.edx):"0"(code));
	return r;
}

// Return CPU vendor name
u32 cpu_vendor_name(char *name)
{
		regs_t r = cpu_cpuid(0x00);
		
		char line1[5];
		line1[0] = ((char *) &r.ebx)[0];
		line1[1] = ((char *) &r.ebx)[1];
		line1[2] = ((char *) &r.ebx)[2];
		line1[3] = ((char *) &r.ebx)[3];
		line1[4] = '\0';

		char line2[5];
		line2[0] = ((char *) &r.ecx)[0];
		line2[1] = ((char *) &r.ecx)[1];
		line2[2] = ((char *) &r.ecx)[2];
		line2[3] = ((char *) &r.ecx)[3];
		line2[4] = '\0';
		
		char line3[5];
		line3[0] = ((char *) &r.edx)[0];
		line3[1] = ((char *) &r.edx)[1];
		line3[2] = ((char *) &r.edx)[2];
		line3[3] = ((char *) &r.edx)[3];
		line3[4] = '\0';
							
		strcpy(name, line1);
		strcat(name, line3);
		strcat(name, line2);
		return 15;
}

// Schedule a process, similar to function in the proocess.cc file
void schedule();

idtdesc 	kidt[IDTSIZE]; 		
int_desc 	intt[IDTSIZE]; 		
gdtdesc 	kgdt[GDTSIZE];		
tss 		default_tss;
gdtr 		kgdtr;				
idtr 		kidtr; 				
u32 *		stack_ptr=0;

void init_gdt_desc(u32 base, u32 limite, u8 acces, u8 other,struct gdtdesc *desc)
{
	desc->lim0_15 = (limite & 0xffff);
	desc->base0_15 = (base & 0xffff);
	desc->base16_23 = (base & 0xff0000) >> 16;
	desc->acces = acces;
	desc->lim16_19 = (limite & 0xf0000) >> 16;
	desc->other = (other & 0xf);
	desc->base24_31 = (base & 0xff000000) >> 24;
	return;
}

void init_gdt(void)
{

	default_tss.debug_flag = 0x00;
	default_tss.io_map = 0x00;
	default_tss.esp0 = 0x1FFF0;
	default_tss.ss0 = 0x18;

	init_gdt_desc(0x0, 0x0, 0x0, 0x0, &kgdt[0]);
	init_gdt_desc(0x0, 0xFFFFF, 0x9B, 0x0D, &kgdt[1]);	
	init_gdt_desc(0x0, 0xFFFFF, 0x93, 0x0D, &kgdt[2]);	
	init_gdt_desc(0x0, 0x0, 0x97, 0x0D, &kgdt[3]);		

	init_gdt_desc(0x0, 0xFFFFF, 0xFF, 0x0D, &kgdt[4]);	
	init_gdt_desc(0x0, 0xFFFFF, 0xF3, 0x0D, &kgdt[5]);	
	init_gdt_desc(0x0, 0x0, 0xF7, 0x0D, &kgdt[6]);		

	init_gdt_desc((u32) & default_tss, 0x67, 0xE9, 0x00, &kgdt[7]);	

	kgdtr.limite = GDTSIZE * 8;
	kgdtr.base = GDTBASE;

	memcpy((char *) kgdtr.base, (char *) kgdt, kgdtr.limite);

	asm("lgdtl (kgdtr)");

	asm("   movw $0x10, %ax	\n \
            movw %ax, %ds	\n \
            movw %ax, %es	\n \
            movw %ax, %fs	\n \
            movw %ax, %gs	\n \
            ljmp $0x08, $next	\n \
            next:		\n");
			
}


void init_idt_desc(u16 select, u32 offset, u16 type, struct idtdesc *desc)
{
	desc->offset0_15 = (offset & 0xffff);
	desc->select = select;
	desc->type = type;
	desc->offset16_31 = (offset & 0xffff0000) >> 16;
	return;
}

extern void _asm_int_0();
extern void _asm_int_1();
extern void _asm_syscalls();
extern void _asm_exc_GP(void);
extern void _asm_exc_PF(void);
extern void _asm_schedule();

void do_syscalls(int num){
	 u32 ret,ret1,ret2,ret3,ret4;
	 asm("mov %%ebx, %0": "=m"(ret):);
	 asm("mov %%ecx, %0": "=m"(ret1):);
	 asm("mov %%edx, %0": "=m"(ret2):);
	 asm("mov %%edi, %0": "=m"(ret3):);
	 asm("mov %%esi, %0": "=m"(ret4):);
	 //io.print("syscall %d \n",num);
	 /*io.print(" ebx : %x  ",ret);
	 io.print(" ecx : %x \n",ret1);
	 io.print(" edx : %x  ",ret2);
	 io.print(" edi : %x \n",ret3);*/
	   
	 arch.setParam(ret,ret1,ret2,ret3,ret4);
	 asm("cli");
	 asm("mov %%ebp, %0": "=m"(stack_ptr):);

	 
	 syscall.call(num);
	 asm("sti");
}



void isr_kbd_int(void)
{
	u8 i;
	static int lshift_enable;
	static int rshift_enable;
	static int alt_enable;
	static int ctrl_enable;
	do {
		i = io.inb(0x64);
	} while ((i & 0x01) == 0);
	

	i = io.inb(0x60);
	i--;

	if (i < 0x80) {		
		switch (i) {
		case 0x29:
			lshift_enable = 1;
			break;
		case 0x35:
			rshift_enable = 1;
			break;
		case 0x1C:
			ctrl_enable = 1;
			break;
		case 0x37:
			alt_enable = 1;
			break;
		default:
		
				if(alt_enable==1)
				{
					io.putctty(kbdmap[i * 4 + 2]);
					if (&io != io.current_io)
					io.current_io->putctty(kbdmap[i * 4 + 2]);
		 
				}
				else if(lshift_enable == 1 || rshift_enable == 1)
				{
		 
					 io.putctty(kbdmap[i * 4 + 1]);
					 if (&io != io.current_io)
						io.current_io->putctty(kbdmap[i * 4 + 1]);
		 
				}
				else
				{
						  io.putctty(kbdmap[i * 4]);
					 if (&io != io.current_io)
					 io.current_io->putctty(kbdmap[i * 4]);
		 
				}
               break;

			//io.print("sancode: %x \n",i * 4 + (lshift_enable || rshift_enable));
			/*io.putctty(kbdmap[i * 4 + (lshift_enable || rshift_enable)]);	//replac� depuis la 10.4.6
			if (&io != io.current_io)
				io.current_io->putctty(kbdmap[i * 4 + (lshift_enable || rshift_enable)]);*/
			break;
		}
	} else {		
		i -= 0x80;
		switch (i) {
		case 0x29:
			lshift_enable = 0;
			break;
		case 0x35:
			rshift_enable = 0;
			break;
		case 0x1C:
			ctrl_enable = 0;
			break;
		case 0x37:
			alt_enable = 0;
			break;
		}
	}
	
		io.outb(0x20,0x20);
		io.outb(0xA0,0x20); 
}


void isr_default_int(int id)
{
	static int tic = 0;
	static int sec = 0;
	switch (id){
		case 1:
			isr_kbd_int();
			break;
			
			
		default:
			return;
		
	}
	
	io.outb(0x20,0x20);
	io.outb(0xA0,0x20);
}


void isr_schedule_int()
{
	static int tic = 0;
	static int sec = 0;
		tic++;
		if (tic % 100 == 0) {
		sec++;
		tic = 0;
	}
	schedule();
	io.outb(0x20,0x20);
	io.outb(0xA0,0x20);
}

void isr_GP_exc(void)
{
	io.print("\n General protection fault !\n");
	if (arch.pcurrent!=NULL){
		io.print("The processes %s have to be killed !\n\n",(arch.pcurrent)->getName());
		(arch.pcurrent)->exit();
		schedule();
	}
	else{
		io.print("The kernel has to be killed !\n\n");
		asm("hlt");
	}
}

void isr_PF_exc(void)
{
	u32 faulting_addr, code;
	u32 eip;
	struct page *pg;
	u32 stack;
 	asm(" 	movl 60(%%ebp), %%eax	\n \
    		mov %%eax, %0		\n \
		mov %%cr2, %%eax	\n \
		mov %%eax, %1		\n \
 		movl 56(%%ebp), %%eax	\n \
    		mov %%eax, %2"
		: "=m"(eip), "=m"(faulting_addr), "=m"(code));
	 asm("mov %%ebp, %0": "=m"(stack):);
	
	//io.print("#PF : %x \n",faulting_addr);
	
	//for (;;);
		if (arch.pcurrent==NULL)
			return;
			
		process_st* current=arch.pcurrent->getPInfo();

	if (faulting_addr >= USER_OFFSET && faulting_addr <= USER_STACK) {
		pg = (struct page *) kmalloc(sizeof(struct page));
		pg->p_addr = get_page_frame();
		pg->v_addr = (char *) (faulting_addr & 0xFFFFF000);
		list_add(&pg->list, &current->pglist);
		pd_add_page(pg->v_addr, pg->p_addr, PG_USER, current->pd);
	}
	else {
		io.print("\n");
		io.print("No autorized memory acces on : %p (eip:%p,code:%p)\n", faulting_addr,eip,  code);
		io.print("heap=%x, heap_limit=%x, stack=%x\n",kern_heap,KERN_HEAP_LIM,stack);
		
		if (arch.pcurrent!=NULL){
			io.print("The processes %s have to be killed !\n\n",(arch.pcurrent)->getName());
			(arch.pcurrent)->exit();
			schedule();
		}
		else{
			io.print("The kernel has to be killed !\n\n");
			asm("hlt");
		}
	}
		
}

void init_idt(void)
{
	

	int i;
	for (i = 0; i < IDTSIZE; i++) 
		init_idt_desc(0x08, (u32)_asm_schedule, INTGATE, &kidt[i]); // 
	
	init_idt_desc(0x08, (u32) _asm_exc_GP, INTGATE, &kidt[13]);		/* #GP */
	init_idt_desc(0x08, (u32) _asm_exc_PF, INTGATE, &kidt[14]);     /* #PF */
	
	init_idt_desc(0x08, (u32) _asm_schedule, INTGATE, &kidt[32]);
	init_idt_desc(0x08, (u32) _asm_int_1, INTGATE, &kidt[33]);
	
	init_idt_desc(0x08, (u32) _asm_syscalls, TRAPGATE, &kidt[48]);
	init_idt_desc(0x08, (u32) _asm_syscalls, TRAPGATE, &kidt[128]); //48
	
	kidtr.limite = IDTSIZE * 8;
	kidtr.base = IDTBASE;
	
	
	memcpy((char *) kidtr.base, (char *) kidt, kidtr.limite);

	asm("lidtl (kidtr)");
}


void init_pic(void)
{
	io.outb(0x20, 0x11);
	io.outb(0xA0, 0x11);

	io.outb(0x21, 0x20);	
	io.outb(0xA1, 0x70);	

	io.outb(0x21, 0x04);
	io.outb(0xA1, 0x02);

	io.outb(0x21, 0x01);
	io.outb(0xA1, 0x01);

	io.outb(0x21, 0x0);
	io.outb(0xA1, 0x0);
}

#define DEBUG_REG(a) io.print("  %s : %x",#a,p->regs.a)

void schedule(){
	Process* pcurrent=arch.pcurrent;
	Process*plist=arch.plist;
	if (pcurrent==0)
		return;

	if (pcurrent->getPNext() == 0 && plist==pcurrent)	
		return;

	process_st* current=pcurrent->getPInfo();
	process_st *p;
	int i, newpid;

	asm("mov (%%ebp), %%eax; mov %%eax, %0": "=m"(stack_ptr):);
	//asm("mov (%%eip), %%eax; mov %%eax, %0": "=m"(current->regs.eip):);
	
	//io.print("stack_ptr : %x \n",stack_ptr);
		current->regs.eflags = stack_ptr[16];
		current->regs.cs = stack_ptr[15];
		current->regs.eip = stack_ptr[14];
		current->regs.eax = stack_ptr[13];
		current->regs.ecx = stack_ptr[12];
		current->regs.edx = stack_ptr[11];
		current->regs.ebx = stack_ptr[10];
		current->regs.ebp = stack_ptr[8];
		current->regs.esi = stack_ptr[7];
		current->regs.edi = stack_ptr[6];
		current->regs.ds = stack_ptr[5];
		current->regs.es = stack_ptr[4];
		current->regs.fs = stack_ptr[3];
		current->regs.gs = stack_ptr[2];

		if (current->regs.cs != 0x08) {	
			current->regs.esp = stack_ptr[17];
			current->regs.ss = stack_ptr[18];
		} else {	
			current->regs.esp = stack_ptr[9] + 12;	/* vaut : &stack_ptr[17] */
			current->regs.ss = default_tss.ss0;
		}

		current->kstack.ss0 = default_tss.ss0;
		current->kstack.esp0 = default_tss.esp0;
	
	//io.print("schedule %s ",pcurrent->getName());
	pcurrent=pcurrent->schedule();
	p = pcurrent->getPInfo();

	//io.print("to %s \n",pcurrent->getName());
	/*DEBUG_REG(eax);
	DEBUG_REG(ebx);
	DEBUG_REG(ecx);
	DEBUG_REG(edx);*/
	/*DEBUG_REG(esp); io.print("\t");
	DEBUG_REG(ebp);	io.print("\n");*/
	//DEBUG_REG(esi);
	//DEBUG_REG(edi);
	//DEBUG_REG(eip);	io.print("\t");
	/*DEBUG_REG(eflags);
	DEBUG_REG(cs);
	DEBUG_REG(ss);
	DEBUG_REG(ds);
	DEBUG_REG(es);
	DEBUG_REG(fs);
	DEBUG_REG(gs);
	DEBUG_REG(cr3);
	io.print("\n");*/
	
	if (p->regs.cs != 0x08)
		switch_to_task(p, USERMODE);
	else
		switch_to_task(p, KERNELMODE);
}

void switch_to_task(process_st* current, int mode)
{

	u32 kesp, eflags;
	u16 kss, ss, cs;
	int sig;

		if ((sig = dequeue_signal(current->signal))) 
			handle_signal(sig);
	
	default_tss.ss0 = current->kstack.ss0;
	default_tss.esp0 = current->kstack.esp0;
	ss = current->regs.ss;
	cs = current->regs.cs;
	eflags = (current->regs.eflags | 0x200) & 0xFFFFBFFF;
	
	if (mode == USERMODE) {
		kss = current->kstack.ss0;
		kesp = current->kstack.esp0;
	} else {			/* KERNELMODE */
		kss = current->regs.ss;
		kesp = current->regs.esp;
	}
	
	
	//io.print("switch to %x \n",current->regs.eip);

	
	asm("	mov %0, %%ss; \
		mov %1, %%esp; \
		cmp %[KMODE], %[mode]; \
		je nextt; \
		push %2; \
		push %3; \
		nextt: \
		push %4; \
		push %5; \
		push %6; \
		push %7; \
		ljmp $0x08, $do_switch" 
		:: \
		"m"(kss), \
		"m"(kesp), \
		"m"(ss), \
		"m"(current->regs.esp), \
		"m"(eflags), \
		"m"(cs), \
		"m"(current->regs.eip), \
		"m"(current), \
		[KMODE] "i"(KERNELMODE), \
		[mode] "g"(mode)
	    );
	
}


int dequeue_signal(int mask) 
{
	int sig;

	if (mask) {
		sig = 1;
		while (!(mask & 1)) {
			mask = mask >> 1;
			sig++;
		}
	}
	else
		sig = 0;

	return sig;
}

int handle_signal(int sig)
{
	Process* pcurrent=arch.pcurrent;
	if (pcurrent==0)
		return 0;

	process_st* current=pcurrent->getPInfo();
	
	u32 *esp;

	//io.print("signal> handle signal : signal %d for process %d\n", sig, pcurrent->getPid());

	if (current->sigfn[sig] == (void*) SIG_IGN) {
		clear_signal(&(current->signal), sig);
	}
	else if (current->sigfn[sig] == (void*) SIG_DFL) {
		switch(sig) {
			case SIGHUP : case SIGINT : case SIGQUIT : 
				asm("mov %0, %%eax; mov %%eax, %%cr3"::"m"(current->regs.cr3));
				pcurrent->exit();
				break;
			case SIGCHLD : 
				break;
			default :
				clear_signal(&(current->signal), sig);
		}
	}
	else {

		esp = (u32*) current->regs.esp - 20;

		asm("mov %0, %%eax; mov %%eax, %%cr3"::"m"(current->regs.cr3));
		
		esp[19] = 0x0030CD00;
		esp[18] = 0x00000EB8;
		esp[17] = current->kstack.esp0;
		esp[16] = current->regs.ss;
		esp[15] = current->regs.esp;
		esp[14] = current->regs.eflags;
		esp[13] = current->regs.cs;
		esp[12] = current->regs.eip;
		esp[11] = current->regs.eax;
		esp[10] = current->regs.ecx;
		esp[9] = current->regs.edx;
		esp[8] = current->regs.ebx;
		esp[7] = current->regs.ebp;
		esp[6] = current->regs.esi;
		esp[5] = current->regs.edi;
		esp[4] = current->regs.ds;
		esp[3] = current->regs.es;
		esp[2] = current->regs.fs;
		esp[1] = current->regs.gs;

		esp[0] = (u32) &esp[18];


		current->regs.esp = (u32) esp;
		current->regs.eip = (u32) current->sigfn[sig];

		current->sigfn[sig] = (void*) SIG_DFL;
		if (sig != SIGCHLD)
			clear_signal(&(current->signal), sig);
	}

	return 0;
}


}
