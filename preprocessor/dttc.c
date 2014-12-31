/////////////////////////////////////////////////////////////////////////////
//    	This file is part of DTTC.
//    
//    	DTTC is free software: you can redistribute it and/or modify
//    	it under the terms of the GNU General Public License as published by
//    	the Free Software Foundation, either version 3 of the License, or
//    	(at your option) any later version.
//
//    	DTTC is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with DTTC.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
//	Author: Hung-Wei Tseng
//
//	Date: Dec 09 2014
//
//	Description: 
//		The main file of dttc preprocessor
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dttc.h"
int create_tables(char *filename, data_trigger *triggers, skippable_region *blocks, support_thread *threads);
int preprocess(char *filename, data_trigger *triggers, skippable_region *blocks, support_thread *threads);
char *eliminate_redundant_characters(char *line);

int number_of_triggers=0,number_of_regions=0,number_of_threads=0;

int main(int argc, char **argv)
{
  char **input_files;
  int i,j, number_of_input_files=0;
  data_trigger triggers[MAX_DTT_ENTRIES];
  skippable_region blocks[MAX_DTT_ENTRIES];
  support_thread threads[MAX_DTT_ENTRIES];
  if(argc < 2)
    fprintf(stderr, "Usage: dttc list_of_input_files\n");
  else
  {
    input_files = (char **)calloc(argc-1, sizeof(char *));
    for(i = 1; i < argc; i++)
    {
      input_files[i-1]=(char *)calloc(strlen(argv[i])+1,sizeof(char));
      strcpy(input_files[i-1],argv[i]);
      number_of_input_files++;
//      printf("%s\n",input_files[i-1]);
    }
  }
  memset(triggers,0,sizeof(data_trigger)*MAX_DTT_ENTRIES);
  memset(blocks,0,sizeof(skippable_region)*MAX_DTT_ENTRIES);
  memset(threads,0,sizeof(support_thread)*MAX_DTT_ENTRIES);
  /* First Parse: establish the table of data triggers and associated support thread functions. */
  for(i = 0; i < number_of_input_files; i++)
  {
    create_tables(input_files[i], triggers, blocks, threads);
  }

    for(i=0;i<number_of_triggers;i++)
    {
      for(j=0;j<number_of_threads;j++)
      {
        if(strcmp(triggers[i].support_thread,threads[j].name)==0)
        {
          triggers[i].state_variable = &threads[j];
          break;
        }
      }
    }


  /* 2nd phase: generate code */
  for(i = 0; i < number_of_input_files; i++)
  {
    preprocess(input_files[i], triggers, blocks, threads);
  }
  return 0;
}
char *eliminate_redundant_characters(char *line)
{
  int i =0;
  for(i=strlen(line);(line[i]=='\n'||line[i]==' '||line[i]=='\t') && i>0;i--)
  {
    line[i]=0;
  }
  for(i=0;i<strlen(line);i++)
  {
    if(line[i]!=' ' && line[i]!='\t')
      return &line[i];
  }
  return NULL;
}

int create_tables(char *filename, data_trigger *triggers, skippable_region *blocks, support_thread *threads)
{
  FILE *input_file, *output_file;
  char input_buffer[MAX_LINE_LENGTH];
  char *input_line;
  char temp_input_line[MAX_LINE_LENGTH];
  char pragma_line[MAX_LINE_LENGTH];
  char current_function[MAX_LINE_LENGTH];
  char *comment_start;
  char *dtt_start;
  char *function_name_start, *function_name_end, *variable_start, *variable_end; // These are for data triggers
  int inside_comment=0,parse_next_line=0,in_function=0;
  int pragma_type = 0;
  int i,j;
  int quote_stack=0;
    input_file = fopen(filename,"r"); // Reading the original source.
    while(fgets(input_buffer, MAX_LINE_LENGTH-1, input_file) != NULL) // Get source code line
    {
      input_line = eliminate_redundant_characters(input_buffer);

      if((comment_start = strstr(input_line, "//")) != NULL) // If this line contains a comment
      {
        *comment_start = 0;
      }
      if((comment_start = strstr(input_line, "/*")) != NULL) // If this line contains a start of blocked comment
      {
        memset(temp_input_line, 0, MAX_LINE_LENGTH);
        strcpy(temp_input_line, comment_start);
        *comment_start = 0;
        inside_comment = 1;
      }      
      if((comment_start = strstr(input_line, "*/")) != NULL) // If this line contains an end of blocked comment
      {
        strcpy(temp_input_line, comment_start);
        strcpy(input_line, temp_input_line);
        inside_comment = 0;
      }
      else if((comment_start = strstr(temp_input_line, "*/")) != NULL) // If this line contains an end of blocked comment
      {
        strcpy(input_line, comment_start);
        inside_comment = 0;
      }
      if(inside_comment)
      {
        memset(input_buffer, 0, MAX_LINE_LENGTH);
        continue;
      }
      if(((dtt_start = strstr(input_line, "#pragma DTT")) != NULL) || 
         ((dtt_start = strstr(input_line, "#DTT")) != NULL) ||
         ((dtt_start = strstr(input_line, "#trigger ")) != NULL) ||
         ((dtt_start = strstr(input_line, "#block")) != NULL) ||
         ((dtt_start = strstr(input_line, "#end_block")) != NULL)
        ) // If this line contains a DTT pragma
      {

        // Find out which type of pragma this is.
        if(strstr(dtt_start, "#trigger"))
          pragma_type = DATA_TRIGGER_PRAGMA;
        else if(strstr(dtt_start, "#block"))
          pragma_type = SKIPPABLE_REGION_BEGIN_PRAGMA;
        else if(strstr(dtt_start, "#end_block"))
          pragma_type = SKIPPABLE_REGION_END_PRAGMA;
        else if(strstr(dtt_start, "#DTT"))
          pragma_type = SUPPORT_THREAD_FUNCTION_PRAGMA;

        // Copy the pragma to the separate line
        memset(pragma_line, 0, MAX_LINE_LENGTH);
        strcpy(pragma_line, dtt_start);
        *dtt_start = 0;
        switch(pragma_type)        
        {
          case DATA_TRIGGER_PRAGMA:
            // Get support thread function name
            function_name_start = strchr(pragma_line,' ')+1;
            function_name_end = strchr(pragma_line,'(');
            *function_name_end = 0;
            strcpy(triggers[number_of_triggers].support_thread, function_name_start);
            
            if((variable_end = strchr(input_line,'='))==NULL)
              variable_end = strchr(input_line,';');
            *variable_end = 0;
            variable_end--;
            for(;*variable_end == ' ' && variable_end > input_line; variable_end--)
              *variable_end = 0;
            variable_start = strrchr(input_line,' ');
            strcpy(triggers[number_of_triggers].name, variable_start+1);
            *variable_start = 0;
            variable_start = eliminate_redundant_characters(input_line);
            strcpy(triggers[number_of_triggers].type, variable_start);
            strcpy(triggers[number_of_triggers].filename, filename);
            fprintf(stderr,"<trigger[%d]> (%s, %s, %s, %s)\n",number_of_triggers, triggers[number_of_triggers].type,triggers[number_of_triggers].name,triggers[number_of_triggers].support_thread,triggers[number_of_triggers].filename);
            number_of_triggers++;
          break;
          case SKIPPABLE_REGION_BEGIN_PRAGMA:
            function_name_start = strchr(pragma_line,' ')+1;
            function_name_end = strchr(pragma_line,'\n');
            *function_name_end = 0;
            // stripe ' 's
            function_name_end--;
            for(;*function_name_end == ' ' && function_name_end > pragma_line; function_name_end--)
              *function_name_end = 0;
            strcpy(blocks[number_of_regions].name, function_name_start);
//            fprintf(stderr,"region: %s\n",blocks[number_of_regions].name);
            number_of_regions++;
          break;
          case SKIPPABLE_REGION_END_PRAGMA:
          break;
          case SUPPORT_THREAD_FUNCTION_PRAGMA:
            function_name_start = strchr(pragma_line,' ')+1;
            function_name_end = strchr(pragma_line,'\n');
            *function_name_end = 0;
            strcpy(threads[number_of_threads].skippable_region, function_name_start);
//            fprintf(stderr,"threads: %s ",threads[number_of_threads].skippable_region);
            parse_next_line=1;
          break;
        }
      }
      // This line contains the name of a support thread function.
      else if(parse_next_line)
      {
            if((function_name_start = strchr(input_line,'*')) == NULL)
              function_name_start = strchr(input_line,' ');
            function_name_end = strchr(input_line,'(');
            *function_name_end = 0;
            strcpy(threads[number_of_threads].name, function_name_start+1);
            number_of_threads++;
            parse_next_line=0;
      }
      else ;
      memset(input_buffer, 0, MAX_LINE_LENGTH);
      memset(temp_input_line, 0, MAX_LINE_LENGTH);
    }
    fclose(input_file);

  return 0;
}

int preprocess(char *filename, data_trigger *triggers, skippable_region *blocks, support_thread *threads)
{
  FILE *input_file, *output_file;
  char input_line[MAX_LINE_LENGTH];
  char additional_line[MAX_LINE_LENGTH];
  char temp_input_line[MAX_LINE_LENGTH];
  char pragma_line[MAX_LINE_LENGTH];
  char line_left[MAX_LINE_LENGTH];
  char line_right[MAX_LINE_LENGTH];
  char *comment_start;
  char *dtt_start;
  char *function_name_start, *function_name_end, *variable_start, *variable_end; // These are for data triggers
  char *right_start, *right_end, *variable_name;
  char *preemble = ")*& ";
  char *brkt;
  int inside_comment=0,parse_next_line=0,contain_a_trigger=0;// control variables
  int pragma_type = 0;
  char output_filename[1024];
  int i;
  int variable_declared = 0, dtt_initialized=0, in_main=0, quote_stack = 0;
  strcpy(output_filename,filename);
  dtt_start = strstr(output_filename, ".dtt");
  strcpy(dtt_start,".c");
  output_file = fopen(output_filename,"w"); // Reading the original source.
  input_file = fopen(filename,"r"); // Reading the original source.
  while(fgets(input_line, MAX_LINE_LENGTH-1, input_file) != NULL) // Get source code line
  {
      if((comment_start = strstr(input_line, "//")) != NULL) // If this line contains a comment
      {
        *comment_start = 0;
      }
      if((comment_start = strstr(input_line, "/*")) != NULL) // If this line contains a start of blocked comment
      {
        memset(temp_input_line, 0, MAX_LINE_LENGTH);
        strcpy(temp_input_line, comment_start);
        *comment_start = 0;
        inside_comment = 1;
      }
            
      if((comment_start = strstr(input_line, "*/")) != NULL) // If this line contains an end of blocked comment
      {
        strcpy(temp_input_line, comment_start+2);
        strcpy(input_line, temp_input_line);
        inside_comment = 0;
      }
      if((comment_start = strstr(temp_input_line, "*/")) != NULL) // If this line contains an end of blocked comment
      {
        strcat(input_line, comment_start+2);
        inside_comment = 0;
      }
      if(inside_comment)
      {
        memset(input_line, 0, MAX_LINE_LENGTH);
        ;
      }
      if(dtt_initialized == 0)
      {
        if(strstr(input_line," main(")!=NULL || strstr(input_line," main (")!=NULL)
        {
          in_main = 1;
        }
      }      
//      fprintf(output_file,"%s\n",input_line);
      if(((dtt_start = strstr(input_line, "#pragma DTT")) != NULL) || 
         ((dtt_start = strstr(input_line, "#DTT")) != NULL) ||
         ((dtt_start = strstr(input_line, "#trigger ")) != NULL) ||
         ((dtt_start = strstr(input_line, "#block")) != NULL) ||
         ((dtt_start = strstr(input_line, "#end_block")) != NULL)
        ) // If this line contains a DTT pragma
      {
        // Find out which type of pragma this is.
        if(strstr(dtt_start, "#trigger"))
          pragma_type = DATA_TRIGGER_PRAGMA;
        else if(strstr(dtt_start, "#block"))
          pragma_type = SKIPPABLE_REGION_BEGIN_PRAGMA;
        else if(strstr(dtt_start, "#end_block"))
          pragma_type = SKIPPABLE_REGION_END_PRAGMA;
        else if(strstr(dtt_start, "#DTT"))
          pragma_type = SUPPORT_THREAD_FUNCTION_PRAGMA;
//        printf("%s %d\n",input_line,pragma_type);
        // Copy the pragma to the separate line
        memset(pragma_line, 0, MAX_LINE_LENGTH);
        strcpy(pragma_line, dtt_start);
        //stripping special characters
        for(i=strlen(pragma_line)-1; pragma_line[i] != 0 && pragma_line[i] != '\n' && pragma_line[i] != '\r' && i >= 0; i--)
          pragma_line[i] = 0;
        *dtt_start = 0;
        switch(pragma_type)        
        {
          case DATA_TRIGGER_PRAGMA:
            strcpy(additional_line, input_line);
            // Create the variable to store old value
            if((variable_end = strchr(additional_line,'='))==NULL)
              variable_end = strchr(additional_line,';');
            *variable_end = 0;
            // stripe ' 's
            variable_end--;
            for(;*variable_end == ' ' && variable_end > additional_line; variable_end--)
              *variable_end = 0;
            variable_start = strrchr(additional_line,' ');
            strcat(additional_line, "__dtt_old_value;");
            fprintf(output_file,"%s\n",additional_line);
          break;
          case SKIPPABLE_REGION_BEGIN_PRAGMA:
            variable_end = strchr(pragma_line,'\n');
//            *variable_end = 0;
            for(;(*variable_end == ' '||*variable_end == '\n') && variable_end > pragma_line; variable_end--)
              *variable_end = 0;
            variable_start = strrchr(pragma_line,' ');
//            variable_start = strrchr(pragma_line,' ');
            fprintf(output_file,"if(%s__dtt_state.state != 1)\n\t{\n",variable_start);
          break;
          case SKIPPABLE_REGION_END_PRAGMA:
            fprintf(output_file,"\t}\n");
          break;
          case SUPPORT_THREAD_FUNCTION_PRAGMA:
          break;
        }
            fprintf(output_file,"%s\n",input_line);
      }
      // This line contains the name of a support thread function.
      else if(parse_next_line)
      {
            parse_next_line=0;
            fprintf(output_file,"%s\n",input_line);
      }
      else // This is a regular line, need to see if there is any data trigger get modified.
      {
            contain_a_trigger = 0;
            // This line contains assignment
            if((right_start = strchr(input_line, '='))!=NULL)
            {
              if(in_main == 1 && dtt_initialized == 0)
              {
                in_main = 0;
                dtt_initialized = 1;
                fprintf(output_file,"int dtt_initialization_result = dtt_init();\n");
              }
              memset(line_left, 0, MAX_LINE_LENGTH);
              memset(line_right, 0, MAX_LINE_LENGTH);
              strcpy(line_right, right_start+1);
              strncpy(line_left, input_line, right_start - input_line);
              for(i = 0; i < number_of_triggers; i++)
              {
                variable_name = strtok_r(line_left, preemble, &brkt);
                if(strcmp(variable_name, triggers[i].name)==0) // The left value may contain a trigger
                {
//                  fprintf(stderr,"%s %s\n",variable_name, line_right);
                  fprintf(output_file,"%s__dtt_old_value = %s;\n",variable_name,variable_name);
                  fprintf(output_file,"tstore(sizeof(%s),&%s__dtt_state,NULL,%s,&%s,&%s__dtt_old_value);\n",triggers[i].type, triggers[i].state_variable->skippable_region,triggers[i].support_thread,triggers[i].name,triggers[i].name);                  
                  break;
                }
              }
              fprintf(output_file,"%s",input_line);
            }
            else
            {

              fprintf(output_file,"%s",input_line);
              if(variable_declared == 0 && strstr(input_line,"#include")!=NULL)
              {
                fprintf(output_file,"#include <dtt.h>\n");
                for(i=0;i<number_of_regions;i++)
                {
                  fprintf(output_file,"dtt_state %s__dtt_state;\n",blocks[i].name);
                  
                }
                variable_declared = 1;
              }
            }
      }
      memset(input_line, 0, MAX_LINE_LENGTH);
      memset(temp_input_line, 0, MAX_LINE_LENGTH);
    }    
    fclose(input_file);
    fclose(output_file);
  return 0;
}
