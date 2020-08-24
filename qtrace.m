classdef qtrace
   properties (Access = public)
      port
      host
      QS
      user 
      pass
      trace
   end
   methods
      function obj = qtrace(h,p,u,pa)
         if nargin >= 2
            if isnumeric(p)
               obj.port = p;
            else
               error('Value must be numeric')
            end
            obj.user=u;
            obj.host=h; 
            obj.trace=true;
            obj.pass=pa;
            obj.QS=qsend(obj.host, obj.port,obj.user,obj.pass);
         end
      end
      
      function r = initTrace(obj,tab,traceid,tracename,tracer)
         if (obj.trace)
            r = obj.QS.Q('mat_initTrace',tab,traceid,tracename,tracer);
         end
      end

      function r = addTrace(obj,tab,traceid,tracename,tracer)
         if (obj.trace) 
            r = obj.QS.Q('mat_addTrace',tab,traceid,tracename,tracer);
         end
      end
      
      function r = errorTrace(obj,tab,traceid,tracename,tracer,err)
         if (obj.trace)
            r = obj.QS.Q('mat_errorTrace',tab,traceid,tracename,tracer);
            GetCallStack(obj,tab,tracer,err);
            %fprintf ("function  %s errored %s here \n %s \n", "qio_test", err.identifier,callStackString)
      
         end
      end
      
      function r = deleteTrace(obj,tab,traceid)
         if (obj.trace)
            r = obj.QS.Q('mat_deleteTrace',tab,traceid);
         end
      end
      
      function r = gatherTrace(obj,tab)
         if (obj.trace)
            r = obj.QS.Q('mat_gatherTrace',tab);
         end
      end
      
     function r =  stopTrace(obj)
            obj.trace = false;
      end
      
      function callStackString = GetCallStack(obj,tab,tr,errorObject)
         try
            theStack = errorObject.stack;
            callStackString = '';
            trace.stepid = tr.stepid + 1;
            for k = 1 : length(theStack)
               [folder, baseFileName, ext] = fileparts(theStack(k).file);
               baseFileName = sprintf('%s%s', baseFileName, ext);
               trace.fn=theStack(k).name;
               trace.line=theStack(k).line;
               obj.QS.Q('mat_errorTrace',tab,tr.stepid,'qio tracker',trace);
               callStackString = sprintf('%s in file %s, in the function %s, at line %d\n', callStackString, baseFileName, theStack(k).name, theStack(k).line);
               trace.stepid = trace.stepid + 1;
            end
         catch ME
            callStackString = GetCallStack(obj,tab,tr,ME);
            errorMessage = sprintf('Error in program %s.\nTraceback (most recent at top):\n%s\nError Message:\n%s', ...
               mfilename, callStackString, ME.message);
            warning('qTrace Module Error',errorMessage);
         end
      end
   end

end
