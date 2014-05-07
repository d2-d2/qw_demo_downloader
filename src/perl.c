#include <EXTERN.h>
#include <perl.h>

#include "quakedef.h"

EXTERN_C void xs_init (pTHX);

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

void perl_clientstate(pTHX_ AV* cv){

		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		PUTBACK;
		SPAGAIN;
		XPUSHs(sv_2mortal(newSViv(cls.state)));
		PUTBACK;
		FREETMPS;
		LEAVE;
		XPUSHs(sv_2mortal(newSViv(cls.state)));
}

void perl_cprt (pTHX_ AV* cv){

		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		Com_Printf("its a callback !- %s\n",POPp);
		PUTBACK;
		SPAGAIN;
		PUTBACK;
		FREETMPS;
		LEAVE;

}

void perl_cbat (pTHX_ AV* cv){

		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		Cbuf_AddText(POPp);
		PUTBACK;
		SPAGAIN;
		PUTBACK;
		FREETMPS;
		LEAVE;
}



EXTERN_C void
xs_init(pTHX)
{
	char *file = __FILE__;
	dXSUB_SYS;

	/* DynaLoader is a special case */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
	newXS("ThinClient::print", perl_cprt, file);
	newXS("ThinClient::addtext", perl_cbat, file);
	newXS("ThinClient::client_state", perl_clientstate, file);
}


static PerlInterpreter *my_perl;



static void call_PerlFrame(float time){

		if (my_perl == NULL)
			return;
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		XPUSHs(sv_2mortal(newSVnv(time)));
		PUTBACK;
		call_pv("frame",G_SCALAR);//G_DISCARD|G_NOARGS);
		SPAGAIN;
		PUTBACK;
		FREETMPS;
		LEAVE;
}


static void call_PerlInit(void){

		if (my_perl == NULL)
			return;
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		PUTBACK;
		call_pv("init",G_DISCARD|G_NOARGS);
		SPAGAIN;
		PUTBACK;
		FREETMPS;
		LEAVE;
}


void call_PerlFunction(void){
		int count;
		if (my_perl == NULL)
			return;
		if (Cmd_Argc() < 2)
			return;

		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		for (count = 1;count < Cmd_Argc();count++)
			XPUSHs(sv_2mortal(newSVpv(Cmd_Argv(count),strlen(Cmd_Argv(count)))));
		PUTBACK;
		call_pv("call_function",G_SCALAR);
		SPAGAIN;
		PUTBACK;
		FREETMPS;
		LEAVE;

}

void call_PerlAddConsole (char	*str){

		if (my_perl == NULL)
			return;
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		XPUSHs(sv_2mortal(newSVpv(str,strlen(str))));
		PUTBACK;
		call_pv("addtoconsole",G_SCALAR);//G_DISCARD|G_NOARGS);
		SPAGAIN;
		PUTBACK;
		FREETMPS;
		LEAVE;

}

void Perl_Frame(void){

	if (my_perl == NULL)
		return;


	call_PerlFrame(Sys_DoubleTime());

}

void Perl_Close(void){
	if (my_perl == NULL)
		return;


	Com_Printf("Closing perl interpreter\n");


	perl_destruct(my_perl);
	perl_free(my_perl);
	my_perl = NULL;
}

void Perl_Con_Init (void){
	char	*my_argv[] = { "", "main.pl"};


	if (my_perl)
		return;



	my_perl		=	perl_alloc();	
	perl_construct(my_perl);
	perl_parse(my_perl,xs_init,2,my_argv,NULL);
	Com_Printf("Initialized perl interpreter\n");
	call_PerlInit();


}

void Perl_Init(void){

	char	*my_argv[] = { "", "main.pl"};
	my_perl		=	perl_alloc();	
	perl_construct(my_perl);
	perl_parse(my_perl,xs_init,2,my_argv,NULL);
	call_PerlInit();

	Cmd_AddCommand("perl_close",Perl_Close);
	Cmd_AddCommand("perl_init",Perl_Con_Init);
	Cmd_AddCommand("perl_call",call_PerlFunction);
	Com_Printf("Initialized perl interpreter\n");
}

