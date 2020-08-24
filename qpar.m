classdef qpar
   properties
      port
      host
      user
      pass
      QS
      slave_rank
      slave_type
      slave_mode
      slave_id
      qio_obj
      qt
      pid
   end
   methods
      function obj = qpar(h, p, u, ps, sl, p_slave_id)
            if isnumeric(p)
               obj.port = p;
            else
               error('Value must be numeric %d', p);
            end
            obj.host=h;obj.user=u;obj.pass=ps; 
            obj.slave_type=sl;
            obj.QS=qsend(obj.host, obj.port, obj.user, obj.pass);
            obj.pid = int32(feature('getpid'));                         
            if p_slave_id == 0  % Interactive register a slave-id
                obj.slave_id = obj.QS.Q('mat_createSlave',sl, int32(0)); 
                obj.slave_mode = 0;
            else
                obj.slave_id = p_slave_id;
                obj.slave_mode = 1;
            end
            if ischar(obj.slave_id) 
                obj.slave_id=int32(str2num(obj.slave_id));
            end
           port_list = obj.QS.Q('mat_updateSlave',obj.slave_id,obj.pid)
           fprintf ("Slave PID: %d \n",obj.pid);
           obj.qio_obj=qio(h,port_list.qio_port,u,ps,'');
           obj.qt=qtrace(h,port_list.qtrace_port,u,ps);
      end
      
      function callStackString = GetCallStack(obj,errorObject)
         try
            theStack = errorObject.stack;
            callStackString = '';
            for k = 1 : length(theStack)-3
               [folder, baseFileName, ext] = fileparts(theStack(k).file);
               baseFileName = sprintf('%s%s', baseFileName, ext);
               callStackString = sprintf('%s in file %s, in the function %s, at line %d\n', callStackString, baseFileName, theStack(k).name, theStack(k).line);
            end
         catch ME
            callStackString = GetCallStack(obj,ME);
            errorMessage = sprintf('Error in program %s.\nTraceback (most recent at top):\n%s\nError Message:\n%s', ...
               mfilename, callStackString, ME.message);
            WarnUser(errorMessage);
         end
      end
      function r = GetCallStack1(obj,errorObject)
         r = sprintf("test %s",errorObject.message);
      end   

      function r = getTask(obj, slave_type)
         r = obj.QS.Q('matlab_getTask',slave_type, obj.slave_id);
      end
      
      function r = slaveListen(obj)
         for n = 1:100000 %Run 100000 times per slave
            fprintf ("Before matlab get task \n");
            messages = obj.QS.Q('matlab_getTask',obj.slave_type,obj.slave_id);
            if isstruct(messages) && messages.task_id==0 && obj.slave_mode == 1 % No pending tasks; Wait
                messages = obj.QS.QB('mat_registerSlave',obj.slave_id,'idle',0,0);
            end    
            if (isstruct(messages) && messages.task_id==0 && obj.slave_mode == 0)
               fprintf ("no tasks - quitting \n");
               break;
            end
            if ( ischar(messages) && strcmp(messages,'quit') ) 
               fprintf ("quitting \n");
               break;
            end
            if istable(messages)
               messages = table2struct(messages);
            elseif isstruct(messages)  
               messages = {messages};
            end
            for n = 1 : length(messages)
              message = messages(n);    
              fprintf ("jobid %d task_id %d \n", message.job_id,message.task_id);
              fprintf ("starting task %d  %s \n",message.task_id, message.function_name);
              startTask(obj,message.job_id,message.task_id);
              %fh = str2func(message.function_name) 
              if (not (strcmp(class(message.rhs),'cell'))) && (isvector(message.rhs) || isnumeric(message.rhs))
                 arg_cell=num2cell(message.rhs);
              else
                 arg_cell=message.rhs;
              end   
              try
                 result=run_slave(obj,message,arg_cell{:});
                 fprintf ("finished task %d  %s \n",message.task_id, message.function_name);
                 uploadSlaveOutput(obj,message.job_id,message.task_id,result);
                 fprintf ("uploaded task %d  %s \n",message.task_id, message.function_name);
              catch ME
                 MERR.identifier = ME.identifier;
                 MERR.message = ME.message;
                 MERR.stack=ME.stack;
                 callStackString = GetCallStack(obj,ME);
                 fprintf ("function  %s errored %s here \n %s \n", message.function_name, ME.identifier,callStackString);
                 setSlaveTaskFailure(obj,message.job_id,message.task_id,MERR);
              end
           end % loop through messages
         end % 100000 passes per slave 
         r=message;
      end %slaveListen
      function r = startTask(obj,job_id,task_id)
         r = obj.QS.Q('mat_startTask',job_id,task_id);
      end
      function r = clearLog(obj)
         r = obj.QS.Q('mat_clearLog');
      end
      function r = uploadSlaveOutput(obj,p_job_id,p_task,p_output)
         r = obj.QS.Q('mat_uploadSlaveOutput',obj.slave_id,p_job_id,p_task,p_output);
      end
      function r = setSlaveTaskFailure(obj,p_job_id,p_task,p_output)
         r = obj.QS.Q('mat_setSlaveTaskFailure',obj.slave_id,p_job_id,p_task,p_output);
      end
      function r = updateExecutable(obj,p_name,p_runtime,p_version,p_functions,p_exec,p_path)
         r = obj.QS.Q('mat_updateExecutable',p_name,p_runtime,p_version,p_functions,p_exec,p_path);
      end
      % End -- These  functions are for use in Slaves


      % Start -- These  functions are for use in Master
      % j = createJob(sched)
      function r = warmUpNodes(obj,p_task_type,p_task_count)
         r = obj.QS.Q('mat_warmUpNodes',p_task_type,p_task_count);
      end
      function r = createJob(obj,p_job_name)
         r = obj.QS.Q('mat_createJob',p_job_name);
      end
      function r = createTask(obj,p_job_id,p_parent_tasks,p_task_type,p_function,p_rhs)
         r = obj.QS.Q('mat_createTask',p_job_id,p_parent_tasks,p_task_type,p_function,p_rhs);
      end
      % Scatter(j); Pass an array of arguments that turn into multiple tasks 
      function r = scatterTaskParm(obj,p_job_id,p_parent_tasks,p_task_type,p_function,p_rhs_list)
         r = obj.QS.Q('mat_scatterTaskParm',p_job_id,p_parent_tasks,p_task_type,p_function,p_rhs_list); 
      end
      function r = waitforTask(obj,p_job_id,p_task_id)
        r = obj.QS.QB('mat_registerSlave',obj.slave_id,'task_waiting',p_job_id,p_task_id);
        if ischar(r) && strcmp(r,'quit')
           fprintf ("quitting \n");
           quit
        end   
      end
      function r = waitforJob(obj,p_job_id)
        r = obj.QS.QB('mat_registerSlave',obj.slave_id,'job_waiting',p_job_id);
        if ischar(r) && strcmp(r,'quit')
           fprintf ("quitting \n");
           quit
        end   
      end
      
     % Submit the distributed job to the local scheduler
      % submit(j);
      function r = submit(obj,p_job_num)
         r = obj.QS.Q('mat_submit',p_job_num);
      end
      % waitForState(j);
      function r = waitForState(obj,j)
         r = obj.QS.Q('mat_waitForState',varargin);
      end
      function r = gatherTask(obj,j,t)
         r = obj.QS.Q('mat_taskGather',j,t);
      end
      % gather(j) - Bring back an array of output values from all the tasks that completed
      function r = gather(obj,j)
         r = obj.QS.Q('mat_taskGather',j,0);
      end
      % gather_status(j) - Bring back an array of Status Values indicating if any failed.
      function r = gatherStatus(obj,j)
         r = obj.QS.Q('mat_gatherStatus',varargin);
      end
      % reduce(j) - Run reduce_expression on all the output values and bring back the result
      function r = reduce(obj,j,reduce_expression)
         r = obj.QS.Q('mat_taskReduce',j,reduce_expression);
      end
      % End -- These  functions are for use in Master
   end
end
