using Irony.Parsing;

namespace Roboteq.MicroBasic.Compiler.Common;

public class CompilerError
{
	private string _message;

	private SourceSpan _span;

	private CompilerErrorLevel _errorLevel;

	public string Message
	{
		get
		{
			return _message;
		}
		set
		{
			_message = value;
		}
	}

	public SourceSpan Span
	{
		get
		{
			return _span;
		}
		set
		{
			_span = value;
		}
	}

	public CompilerErrorLevel ErrorLevel
	{
		get
		{
			return _errorLevel;
		}
		set
		{
			_errorLevel = value;
		}
	}

	public CompilerError(string message, SourceSpan span, CompilerErrorLevel level)
	{
		_message = message;
		_span = span;
		_errorLevel = level;
	}

	public CompilerError(string message, SourceSpan span)
		: this(message, span, CompilerErrorLevel.Error)
	{
	}

	public CompilerError(string message)
		: this(message, default(SourceSpan))
	{
	}

	public override string ToString()
	{
		return $"[{Span.Location.Line + 1}, {Span.Location.Column}] {ErrorLevel}: {Message}";
	}
}
