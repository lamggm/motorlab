using System;
using System.Text;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.CompilerUnits;

namespace Roboteq.MicroBasic.Compiler;

public class MicroBasicCompiler
{
	public void Compile(CompilerContext context, string source)
	{
		int num = 0;
		int num2 = 0;
		string[] array = source.Split(new string[1] { Environment.NewLine }, StringSplitOptions.None);
		for (num = 0; num < array.Length; num++)
		{
			string text = array[num].Trim();
			if (!string.IsNullOrEmpty(text) && !text.StartsWith("'"))
			{
				break;
			}
			num2 += text.Length + 2;
		}
		StringBuilder stringBuilder = new StringBuilder();
		for (int i = num; i < array.Length; i++)
		{
			stringBuilder.AppendLine(array[i]);
		}
		source = stringBuilder.ToString();
		Parser parser = new Parser(new MicroBasicGrammar());
		ParseTree currentParseTree;
		try
		{
			if (!source.EndsWith(Environment.NewLine))
			{
				source += Environment.NewLine;
			}
			parser.Parse(source, "<source>");
		}
		finally
		{
			currentParseTree = parser.Context.CurrentParseTree;
		}
		context.ParseTree = currentParseTree;
		if (currentParseTree == null)
		{
			return;
		}
		foreach (ParserMessage parserMessage in context.ParseTree.ParserMessages)
		{
			if (parserMessage.Level == ParserErrorLevel.Error)
			{
				context.AddError(parserMessage.Message, new SourceSpan(parserMessage.Location, 0));
			}
			else if (parserMessage.Level == ParserErrorLevel.Warning)
			{
				context.AddWarning(parserMessage.Message, new SourceSpan(parserMessage.Location, 0));
			}
		}
		if (currentParseTree.Root == null)
		{
			foreach (CompilerError error in context.Errors)
			{
				error.Span = new SourceSpan(new SourceLocation(error.Span.Location.Position + num2, error.Span.Location.Line + num, error.Span.Location.Column), error.Span.Length);
			}
			return;
		}
		CompilerUnit compilerUnit = currentParseTree.Root.AstNode as CompilerUnit;
		compilerUnit?.InitializeUnit(context);
		compilerUnit?.CreateTokens(context);
		context.GenerateCode();
		foreach (CompilerError error2 in context.Errors)
		{
			error2.Span = new SourceSpan(new SourceLocation(error2.Span.Location.Position + num2, error2.Span.Location.Line + num, error2.Span.Location.Column), error2.Span.Length);
		}
	}
}
