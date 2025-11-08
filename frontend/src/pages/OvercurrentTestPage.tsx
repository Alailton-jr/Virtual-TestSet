import { useState } from 'react'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select'
import { Badge } from '@/components/ui/badge'
import { Play } from 'lucide-react'

interface TestResult {
  current: number
  expectedTime: number
  actualTime: number
  result: 'pass' | 'fail'
}

export default function OvercurrentTestPage() {
  const [pickup, setPickup] = useState('5.0')
  const [timeDial, setTimeDial] = useState('5')
  const [curveType, setCurveType] = useState('IEC_SI')
  const [results, setResults] = useState<TestResult[]>([])

  const handleRunTest = () => {
    setResults([
      { current: 6.0, expectedTime: 2.50, actualTime: 2.48, result: 'pass' },
      { current: 10.0, expectedTime: 0.80, actualTime: 0.79, result: 'pass' },
      { current: 20.0, expectedTime: 0.35, actualTime: 0.36, result: 'pass' },
    ])
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-3xl font-bold tracking-tight">Overcurrent 50/51 Test</h1>
        <p className="text-muted-foreground">
          Automated testing for overcurrent protection relays
        </p>
      </div>

      <div className="grid gap-6 lg:grid-cols-3">
        <Card className="lg:col-span-2">
          <CardHeader><CardTitle>Test Configuration</CardTitle><CardDescription>Configure pickup, time dial, curve type, and test points</CardDescription></CardHeader>
          <CardContent className="space-y-4">
            <div className="grid gap-4 sm:grid-cols-3">
              <div className="space-y-2">
                <Label htmlFor="pickup">Pickup (A)</Label>
                <Input id="pickup" type="number" step="0.1" value={pickup} onChange={(e) => setPickup(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="timeDial">Time Dial</Label>
                <Input id="timeDial" type="number" step="0.1" value={timeDial} onChange={(e) => setTimeDial(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="curve">Curve Type</Label>
                <Select value={curveType} onValueChange={setCurveType}>
                  <SelectTrigger id="curve"><SelectValue /></SelectTrigger>
                  <SelectContent>
                    <SelectItem value="IEC_SI">IEC Standard Inverse</SelectItem>
                    <SelectItem value="IEC_VI">IEC Very Inverse</SelectItem>
                    <SelectItem value="IEC_EI">IEC Extremely Inverse</SelectItem>
                    <SelectItem value="IEEE_MI">IEEE Moderately Inverse</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>
            <Button onClick={handleRunTest} className="w-full"><Play className="mr-2 h-4 w-4" />Run Test</Button>
            {results.length > 0 && (
              <div className="space-y-2">
                <Label>Test Results</Label>
                {results.map((r, idx) => (
                  <div key={idx} className="flex items-center justify-between p-2 border rounded text-sm">
                    <span className="font-mono">{r.current}A</span>
                    <span className="text-muted-foreground">Exp: {r.expectedTime.toFixed(2)}s / Act: {r.actualTime.toFixed(2)}s</span>
                    <Badge variant={r.result === 'pass' ? 'default' : 'destructive'}>{r.result}</Badge>
                  </div>
                ))}
              </div>
            )}
          </CardContent>
        </Card>

        <Card>
          <CardHeader><CardTitle>Time-Current Curve</CardTitle><CardDescription>Expected vs actual</CardDescription></CardHeader>
          <CardContent>
            <div className="h-80 flex items-center justify-center border rounded-lg bg-muted/20">
              <p className="text-sm text-muted-foreground">TCC curve visualization coming soon...</p>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  )
}
